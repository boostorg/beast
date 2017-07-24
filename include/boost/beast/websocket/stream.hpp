//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_STREAM_HPP
#define BOOST_BEAST_WEBSOCKET_STREAM_HPP

#include <boost/beast/config.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/beast/websocket/option.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/detail/frame.hpp>
#include <boost/beast/websocket/detail/hybi13.hpp>
#include <boost/beast/websocket/detail/mask.hpp>
#include <boost/beast/websocket/detail/pausation.hpp>
#include <boost/beast/websocket/detail/pmd_extension.hpp>
#include <boost/beast/websocket/detail/utf8_checker.hpp>
#include <boost/beast/core/async_result.hpp>
#include <boost/beast/core/buffered_read_stream.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/static_buffer.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/detail/type_traits.hpp>
#include <boost/beast/zlib/deflate_stream.hpp>
#include <boost/beast/zlib/inflate_stream.hpp>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>

namespace boost {
namespace beast {
namespace websocket {

namespace detail {
class frame_test;
}

/// The type of object holding HTTP Upgrade requests
using request_type = http::request<http::empty_body>;

/// The type of object holding HTTP Upgrade responses
using response_type = http::response<http::string_body>;

/** The type of received control frame.

    Values of this type are passed to the control frame
    callback set using @ref stream::control_callback.
*/
enum class frame_type
{
    /// A close frame was received
    close,

    /// A ping frame was received
    ping,

    /// A pong frame was received
    pong
};

//--------------------------------------------------------------------

/** Provides message-oriented functionality using WebSocket.

    The @ref stream class template provides asynchronous and blocking
    message-oriented functionality necessary for clients and servers
    to utilize the WebSocket protocol.
    
    For asynchronous operations, the application must ensure
    that they are are all performed within the same implicit
    or explicit strand.

    @par Thread Safety
    @e Distinct @e objects: Safe.@n
    @e Shared @e objects: Unsafe.

    @par Example

    To use the @ref stream template with an `ip::tcp::socket`,
    you would write:

    @code
    websocket::stream<ip::tcp::socket> ws{io_service};
    @endcode
    Alternatively, you can write:
    @code
    ip::tcp::socket sock{io_service};
    websocket::stream<ip::tcp::socket&> ws{sock};
    @endcode

    @tparam NextLayer The type representing the next layer, to which
    data will be read and written during operations. For synchronous
    operations, the type must support the @b SyncStream concept.
    For asynchronous operations, the type must support the
    @b AsyncStream concept.

    @note A stream object must not be moved or destroyed while there
    are pending asynchronous operations associated with it.

    @par Concepts
        @b AsyncStream,
        @b DynamicBuffer,
        @b SyncStream
*/
template<class NextLayer>
class stream
{
    friend class detail::frame_test;
    friend class stream_test;
    friend class frame_test;

    /*  The read buffer has to be at least as large
        as the largest possible control frame including
        the frame header.
    */
    static std::size_t constexpr max_control_frame_size = 2 + 8 + 4 + 125;
    static std::size_t constexpr tcp_frame_size = 1536;

    struct op {};

    // tokens are used to order reads and writes
    class token
    {
        unsigned char id_ = 1;
        explicit token(unsigned char id) : id_(id) {}
    public:
        token() = default;
        token(token const&) = default;
        operator bool() const { return id_ != 0; }
        bool operator==(token const& t) { return id_ == t.id_; }
        bool operator!=(token const& t) { return id_ != t.id_; }
        token unique() { token t{id_++}; if(id_ == 0) ++id_; return t; }
        void reset() { id_ = 0; }
    };

    using control_cb_type =
        std::function<void(frame_type, string_view)>;

    /// Identifies the role of a WebSockets stream.
    enum class role_type
    {
        /// Stream is operating as a client.
        client,

        /// Stream is operating as a server.
        server
    };

    // State information for the message being received
    //
    struct rd_t
    {
        detail::frame_header fh;        // current frame header
        detail::prepared_key key;       // current stateful mask key
        std::uint64_t size;             // total size of current message so far
        std::uint64_t remain;           // message frame bytes left in current frame
        detail::frame_streambuf fb;     // to write control frames
        detail::utf8_checker utf8;      // to validate utf8

        // A small, circular buffer to read frame headers.
        // This improves performance by avoiding small reads.
        static_buffer<+tcp_frame_size> buf;

        // opcode of current message being read
        detail::opcode op;

        // `true` if the next frame is a continuation.
        bool cont;

        bool done;                      // set when a message is done
    };

    // State information for the message being sent
    //
    struct wr_t
    {
        // `true` if next frame is a continuation,
        // `false` if next frame starts a new message
        bool cont;

        // `true` if this message should be auto-fragmented
        // This gets set to the auto-fragment option at the beginning
        // of sending a message, so that the option can be changed
        // mid-send without affecting the current message.
        bool autofrag;

        // `true` if this message should be compressed.
        // This gets set to the compress option at the beginning of
        // of sending a message, so that the option can be changed
        // mid-send without affecting the current message.
        bool compress;

        // Size of the write buffer.
        // This gets set to the write buffer size option at the
        // beginning of sending a message, so that the option can be
        // changed mid-send without affecting the current message.
        std::size_t buf_size;

        // The write buffer. Used for compression and masking.
        // The buffer is allocated or reallocated at the beginning of
        // sending a message.
        std::unique_ptr<std::uint8_t[]> buf;
    };

    // State information for the permessage-deflate extension
    struct pmd_t
    {
        // `true` if current read message is compressed
        bool rd_set;

        zlib::deflate_stream zo;
        zlib::inflate_stream zi;
    };

    buffered_read_stream<
        NextLayer, flat_buffer> stream_;    // the wrapped stream
    detail::maskgen maskgen_;               // source of mask keys
    std::size_t rd_msg_max_ =
        16 * 1024 * 1024;                   // max message size
    bool wr_autofrag_ = true;               // auto fragment
    std::size_t wr_buf_size_ = 4096;        // write buffer size
    std::size_t rd_buf_size_ = 4096;        // read buffer size
    detail::opcode wr_opcode_ =
        detail::opcode::text;               // outgoing message type
    control_cb_type ctrl_cb_;               // control callback
    role_type role_;                        // server or client
    bool failed_;                           // the connection failed

    bool rd_close_;                         // read close frame
    bool wr_close_;                         // sent close frame
    token wr_block_;                        // op currenly writing

    ping_data* ping_data_;                  // where to put the payload
    detail::pausation rd_op_;               // paused read op
    detail::pausation wr_op_;               // paused write op
    detail::pausation ping_op_;             // paused ping op
    detail::pausation close_op_;            // paused close op
    close_reason cr_;                       // set from received close frame
    rd_t rd_;                               // read state
    wr_t wr_;                               // write state

    // If not engaged, then permessage-deflate is not
    // enabled for the currently active session.
    std::unique_ptr<pmd_t> pmd_;

    // Local options for permessage-deflate
    permessage_deflate pmd_opts_;

    // Offer for clients, negotiated result for servers
    detail::pmd_offer pmd_config_;

    token t_;

public:
    /// The type of the next layer.
    using next_layer_type =
        typename std::remove_reference<NextLayer>::type;

    /// The type of the lowest layer.
    using lowest_layer_type =
        typename get_lowest_layer<next_layer_type>::type;

    /** Destructor

        Destroys the stream and all associated resources.

        @note A stream object must not be destroyed while there
        are pending asynchronous operations associated with it.
    */
    ~stream() = default;

    /** Constructor

        If `NextLayer` is move constructible, this function
        will move-construct a new stream from the existing stream.

        @note The behavior of move assignment on or from streams
        with active or pending operations is undefined.
    */
    stream(stream&&) = default;

    /** Assignment

        If `NextLayer` is move assignable, this function
        will move-assign a new stream from the existing stream.

        @note The behavior of move assignment on or from streams
        with active or pending operations is undefined.
    */
    stream& operator=(stream&&) = default;

    /** Constructor

        This constructor creates a websocket stream and initializes
        the next layer object.

        @throws Any exceptions thrown by the NextLayer constructor.

        @param args The arguments to be passed to initialize the
        next layer object. The arguments are forwarded to the next
        layer's constructor.
    */
    template<class... Args>
    explicit
    stream(Args&&... args);

    //--------------------------------------------------------------------------

    /** Return the `io_service` associated with the stream

        This function may be used to obtain the `io_service` object
        that the stream uses to dispatch handlers for asynchronous
        operations.

        @return A reference to the io_service object that the stream
        will use to dispatch handlers. 
    */
    boost::asio::io_service&
    get_io_service()
    {
        return stream_.get_io_service();
    }

    /** Get a reference to the next layer

        This function returns a reference to the next layer
        in a stack of stream layers.

        @return A reference to the next layer in the stack of
        stream layers.
    */
    next_layer_type&
    next_layer()
    {
        return stream_.next_layer();
    }

    /** Get a reference to the next layer

        This function returns a reference to the next layer in a
        stack of stream layers.

        @return A reference to the next layer in the stack of
        stream layers.
    */
    next_layer_type const&
    next_layer() const
    {
        return stream_.next_layer();
    }

    /** Get a reference to the lowest layer

        This function returns a reference to the lowest layer
        in a stack of stream layers.

        @return A reference to the lowest layer in the stack of
        stream layers.
    */
    lowest_layer_type&
    lowest_layer()
    {
        return stream_.lowest_layer();
    }

    /** Get a reference to the lowest layer

        This function returns a reference to the lowest layer
        in a stack of stream layers.

        @return A reference to the lowest layer in the stack of
        stream layers. Ownership is not transferred to the caller.
    */
    lowest_layer_type const&
    lowest_layer() const
    {
        return stream_.lowest_layer();
    }

    //--------------------------------------------------------------------------
    //
    // Observers
    //
    //--------------------------------------------------------------------------

    /** Returns `true` if the latest message data indicates binary.

        This function informs the caller of whether the last
        received message frame represents a message with the
        binary opcode.

        If there is no last message frame, the return value is
        undefined.
    */
    bool
    got_binary() const
    {
        return rd_.op == detail::opcode::binary;
    }

    /** Returns `true` if the latest message data indicates text.

        This function informs the caller of whether the last
        received message frame represents a message with the
        text opcode.

        If there is no last message frame, the return value is
        undefined.
    */
    bool
    got_text() const
    {
        return ! got_binary();
    }

    /// Returns `true` if the last completed read finished the current message.
    bool
    is_message_done() const
    {
        return rd_.done;
    }

    /** Returns the close reason received from the peer.

        This is only valid after a read completes with error::closed.
    */
    close_reason const&
    reason() const
    {
        return cr_;
    }

    /** Returns a suggested maximum buffer size for the next call to read.

        This function returns a reasonable upper limit on the number
        of bytes for the size of the buffer passed in the next call
        to read. The number is determined by the state of the current
        frame and whether or not the permessage-deflate extension is
        enabled.

        @param initial_size A size representing the caller's desired
        buffer size for when there is no information which may be used
        to calculate a more specific value. For example, when reading
        the first frame header of a message.
    */
    std::size_t
    read_size_hint(
        std::size_t initial_size = +tcp_frame_size) const;

    /** Returns a suggested maximum buffer size for the next call to read.

        This function returns a reasonable upper limit on the number
        of bytes for the size of the buffer passed in the next call
        to read. The number is determined by the state of the current
        frame and whether or not the permessage-deflate extension is
        enabled.

        @param buffer The buffer which will be used for reading. The
        implementation will query the buffer to obtain the optimum
        size of a subsequent call to `buffer.prepare` based on the
        state of the current frame, if any.
    */
    template<class DynamicBuffer
#if ! BOOST_BEAST_DOXYGEN
        , class = typename std::enable_if<
            ! std::is_integral<DynamicBuffer>::value>::type
#endif
    >
    std::size_t
    read_size_hint(
        DynamicBuffer& buffer) const;

    //--------------------------------------------------------------------------
    //
    // Settings
    //
    //--------------------------------------------------------------------------

    /// Set the permessage-deflate extension options
    void
    set_option(permessage_deflate const& o);

    /// Get the permessage-deflate extension options
    void
    get_option(permessage_deflate& o)
    {
        o = pmd_opts_;
    }

    /** Set the automatic fragmentation option.

        Determines if outgoing message payloads are broken up into
        multiple pieces.

        When the automatic fragmentation size is turned on, outgoing
        message payloads are broken up into multiple frames no larger
        than the write buffer size.

        The default setting is to fragment messages.

        @param value A `bool` indicating if auto fragmentation should be on.

        @par Example
        Setting the automatic fragmentation option:
        @code
            ws.auto_fragment(true);
        @endcode
    */
    void
    auto_fragment(bool value)
    {
        wr_autofrag_ = value;
    }

    /// Returns `true` if the automatic fragmentation option is set.
    bool
    auto_fragment() const
    {
        return wr_autofrag_;
    }

    /** Set the binary message option.

        This controls whether or not outgoing message opcodes
        are set to binary or text. The setting is only applied
        at the start when a caller begins a new message. Changing
        the opcode after a message is started will only take effect
        after the current message being sent is complete.

        The default setting is to send text messages.

        @param value `true` if outgoing messages should indicate
        binary, or `false` if they should indicate text.

        @par Example
        Setting the message type to binary.
        @code
            ws.binary(true);
        @endcode
        */
    void
    binary(bool value)
    {
        wr_opcode_ = value ?
            detail::opcode::binary :
            detail::opcode::text;
    }

    /// Returns `true` if the binary message option is set.
    bool
    binary() const
    {
        return wr_opcode_ == detail::opcode::binary;
    }

    /** Set the control frame callback.

        Sets the callback to be invoked whenever a ping, pong,
        or close control frame is received during a call to one
        of the following functions:

        @li @ref beast::websocket::stream::read
        @li @ref beast::websocket::stream::read_some
        @li @ref beast::websocket::stream::async_read
        @li @ref beast::websocket::stream::async_read_some

        Unlike completion handlers, the callback will be invoked
        for each control frame during a call to any synchronous
        or asynchronous read function. The operation is passive,
        with no associated error code, and triggered by reads.

        The signature of the callback must be:
        @code
        void
        callback(
            frame_type kind,       // The type of frame
            string_view payload    // The payload in the frame
        );
        @endcode

        For close frames, the close reason code may be obtained by
        calling the function @ref reason.

        If the read operation which receives the control frame is
        an asynchronous operation, the callback will be invoked using
        the same method as that used to invoke the final handler.

        @note It is not necessary to send a close frame upon receipt
        of a close frame. The implementation does this automatically.
        Attempting to send a close frame after a close frame is
        received will result in undefined behavior.

        @param cb The callback to set.
    */
    void
    control_callback(
        std::function<void(frame_type, string_view)> cb)
    {
        ctrl_cb_ = std::move(cb);
    }

    /** Set the maximum incoming message size option.

        Sets the largest permissible incoming message size. Message
        frame fields indicating a size that would bring the total
        message size over this limit will cause a protocol failure.

        The default setting is 16 megabytes. A value of zero indicates
        a limit of the maximum value of a `std::uint64_t`.

        @par Example
        Setting the maximum read message size.
        @code
            ws.read_message_max(65536);
        @endcode

        @param amount The limit on the size of incoming messages.
    */
    void
    read_message_max(std::size_t amount)
    {
        rd_msg_max_ = amount;
    }

    /// Returns the maximum incoming message size setting.
    std::size_t
    read_message_max() const
    {
        return rd_msg_max_;
    }

    /** Set the write buffer size option.

        Sets the size of the write buffer used by the implementation to
        send frames. The write buffer is needed when masking payload data
        in the client role, compressing frames, or auto-fragmenting message
        data.

        Lowering the size of the buffer can decrease the memory requirements
        for each connection, while increasing the size of the buffer can reduce
        the number of calls made to the next layer to write data.

        The default setting is 4096. The minimum value is 8.

        The write buffer size can only be changed when the stream is not
        open. Undefined behavior results if the option is modified after a
        successful WebSocket handshake.

        @par Example
        Setting the write buffer size.
        @code
            ws.write_buffer_size(8192);
        @endcode

        @param amount The size of the write buffer in bytes.
    */
    void
    write_buffer_size(std::size_t amount)
    {
        if(amount < 8)
            BOOST_THROW_EXCEPTION(std::invalid_argument{
                "write buffer size underflow"});
        wr_buf_size_ = amount;
    };

    /// Returns the size of the write buffer.
    std::size_t
    write_buffer_size() const
    {
        return wr_buf_size_;
    }

    /** Set the text message option.

        This controls whether or not outgoing message opcodes
        are set to binary or text. The setting is only applied
        at the start when a caller begins a new message. Changing
        the opcode after a message is started will only take effect
        after the current message being sent is complete.

        The default setting is to send text messages.

        @param value `true` if outgoing messages should indicate
        text, or `false` if they should indicate binary.

        @par Example
        Setting the message type to text.
        @code
            ws.text(true);
        @endcode
    */
    void
    text(bool value)
    {
        wr_opcode_ = value ?
            detail::opcode::text :
            detail::opcode::binary;
    }

    /// Returns `true` if the text message option is set.
    bool
    text() const
    {
        return wr_opcode_ == detail::opcode::text;
    }

    //--------------------------------------------------------------------------
    //
    // Handshaking
    //
    //--------------------------------------------------------------------------

    /** Read and respond to a WebSocket HTTP Upgrade request.

        This function is used to synchronously read an HTTP WebSocket
        Upgrade request and send the HTTP response. The call blocks
        until one of the following conditions is true:

        @li The request is received and the response finishes sending.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to
        the next layer's `read_some` and `write_some` functions.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When this call returns, the stream is then ready to send and
        receive WebSocket protocol frames and messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure.

        @throws system_error Thrown on failure.
    */
    void
    accept();

    /** Read and respond to a WebSocket HTTP Upgrade request.

        This function is used to synchronously read an HTTP WebSocket
        Upgrade request and send the HTTP response. The call blocks
        until one of the following conditions is true:

        @li The request is received and the response finishes sending.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to
        the next layer's `read_some` and `write_some` functions.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When this call returns, the stream is then ready to send and
        receive WebSocket protocol frames and messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure.

        @param decorator A function object which will be called to modify
        the HTTP response object delivered by the implementation. This
        could be used to set the Server field, subprotocols, or other
        application or HTTP specific fields. The object will be called
        with this equivalent signature:
        @code void decorator(
            response_type& res
        ); @endcode

        @throws system_error Thrown on failure.
    */
    template<class ResponseDecorator>
    void
    accept_ex(ResponseDecorator const& decorator);

    /** Read and respond to a WebSocket HTTP Upgrade request.

        This function is used to synchronously read an HTTP WebSocket
        Upgrade request and send the HTTP response. The call blocks
        until one of the following conditions is true:

        @li The request is received and the response finishes sending.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to
        the next layer's `read_some` and `write_some` functions.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When this call returns, the stream is then ready to send and
        receive WebSocket protocol frames and messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure.

        @param ec Set to indicate what error occurred, if any.
    */
    void
    accept(error_code& ec);

    /** Read and respond to a WebSocket HTTP Upgrade request.

        This function is used to synchronously read an HTTP WebSocket
        Upgrade request and send the HTTP response. The call blocks
        until one of the following conditions is true:

        @li The request is received and the response finishes sending.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to
        the next layer's `read_some` and `write_some` functions.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When this call returns, the stream is then ready to send and
        receive WebSocket protocol frames and messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure.

        @param decorator A function object which will be called to modify
        the HTTP response object delivered by the implementation. This
        could be used to set the Server field, subprotocols, or other
        application or HTTP specific fields. The object will be called
        with this equivalent signature:
        @code void decorator(
            response_type& res
        ); @endcode

        @param ec Set to indicate what error occurred, if any.
    */
    template<class ResponseDecorator>
    void
    accept_ex(
        ResponseDecorator const& decorator,
        error_code& ec);

    /** Read and respond to a WebSocket HTTP Upgrade request.

        This function is used to synchronously read an HTTP WebSocket
        Upgrade request and send the HTTP response. The call blocks
        until one of the following conditions is true:

        @li The request is received and the response finishes sending.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to
        the next layer's `read_some` and `write_some` functions.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When this call returns, the stream is then ready to send and
        receive WebSocket protocol frames and messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure.

        @param buffers Caller provided data that has already been
        received on the stream. The implementation will copy the
        caller provided data before the function returns.

        @throws system_error Thrown on failure.
    */
    template<class ConstBufferSequence>
#if BOOST_BEAST_DOXYGEN
    void
#else
    typename std::enable_if<! http::detail::is_header<
        ConstBufferSequence>::value>::type
#endif
    accept(ConstBufferSequence const& buffers);

    /** Read and respond to a WebSocket HTTP Upgrade request.

        This function is used to synchronously read an HTTP WebSocket
        Upgrade request and send the HTTP response. The call blocks
        until one of the following conditions is true:

        @li The request is received and the response finishes sending.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to
        the next layer's `read_some` and `write_some` functions.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When this call returns, the stream is then ready to send and
        receive WebSocket protocol frames and messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure.

        @param buffers Caller provided data that has already been
        received on the stream. The implementation will copy the
        caller provided data before the function returns.

        @param decorator A function object which will be called to modify
        the HTTP response object delivered by the implementation. This
        could be used to set the Server field, subprotocols, or other
        application or HTTP specific fields. The object will be called
        with this equivalent signature:
        @code void decorator(
            response_type& res
        ); @endcode

        @throws system_error Thrown on failure.
    */
    template<class ConstBufferSequence,
        class ResponseDecorator>
#if BOOST_BEAST_DOXYGEN
    void
#else
    typename std::enable_if<! http::detail::is_header<
        ConstBufferSequence>::value>::type
#endif
    accept_ex(
        ConstBufferSequence const& buffers,
        ResponseDecorator const& decorator);

    /** Read and respond to a WebSocket HTTP Upgrade request.

        This function is used to synchronously read an HTTP WebSocket
        Upgrade request and send the HTTP response. The call blocks
        until one of the following conditions is true:

        @li The request is received and the response finishes sending.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to
        the next layer's `read_some` and `write_some` functions.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When this call returns, the stream is then ready to send and
        receive WebSocket protocol frames and messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure.

        @param buffers Caller provided data that has already been
        received on the stream. The implementation will copy the
        caller provided data before the function returns.

        @param ec Set to indicate what error occurred, if any.
    */
    template<class ConstBufferSequence>
#if BOOST_BEAST_DOXYGEN
    void
#else
    typename std::enable_if<! http::detail::is_header<
        ConstBufferSequence>::value>::type
#endif
    accept(
        ConstBufferSequence const& buffers,
        error_code& ec);

    /** Read and respond to a WebSocket HTTP Upgrade request.

        This function is used to synchronously read an HTTP WebSocket
        Upgrade request and send the HTTP response. The call blocks
        until one of the following conditions is true:

        @li The request is received and the response finishes sending.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to
        the next layer's `read_some` and `write_some` functions.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When this call returns, the stream is then ready to send and
        receive WebSocket protocol frames and messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure.

        @param buffers Caller provided data that has already been
        received on the stream. The implementation will copy the
        caller provided data before the function returns.

        @param decorator A function object which will be called to modify
        the HTTP response object delivered by the implementation. This
        could be used to set the Server field, subprotocols, or other
        application or HTTP specific fields. The object will be called
        with this equivalent signature:
        @code void decorator(
            response_type& res
        ); @endcode

        @param ec Set to indicate what error occurred, if any.
    */
    template<class ConstBufferSequence, class ResponseDecorator>
#if BOOST_BEAST_DOXYGEN
    void
#else
    typename std::enable_if<! http::detail::is_header<
        ConstBufferSequence>::value>::type
#endif
    accept_ex(
        ConstBufferSequence const& buffers,
        ResponseDecorator const& decorator,
        error_code& ec);

    /** Respond to a WebSocket HTTP Upgrade request

        This function is used to synchronously send the HTTP response
        to an HTTP request possibly containing a WebSocket Upgrade.
        The call blocks until one of the following conditions is true:

        @li The response finishes sending.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to
        the next layer's `read_some` and `write_some` functions.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When this call returns, the stream is then ready to send and
        receive WebSocket protocol frames and messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure.

        @param req An object containing the HTTP Upgrade request.
        Ownership is not transferred, the implementation will not
        access this object from other threads.

        @throws system_error Thrown on failure.
    */
    template<class Body, class Allocator>
    void
    accept(http::request<Body,
        http::basic_fields<Allocator>> const& req);

    /** Respond to a WebSocket HTTP Upgrade request

        This function is used to synchronously send the HTTP response
        to an HTTP request possibly containing a WebSocket Upgrade.
        The call blocks until one of the following conditions is true:

        @li The response finishes sending.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to
        the next layer's `read_some` and `write_some` functions.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When this call returns, the stream is then ready to send and
        receive WebSocket protocol frames and messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure.

        @param req An object containing the HTTP Upgrade request.
        Ownership is not transferred, the implementation will not
        access this object from other threads.

        @param decorator A function object which will be called to modify
        the HTTP response object delivered by the implementation. This
        could be used to set the Server field, subprotocols, or other
        application or HTTP specific fields. The object will be called
        with this equivalent signature:
        @code void decorator(
            response_type& res
        ); @endcode

        @throws system_error Thrown on failure.
    */
    template<class Body, class Allocator,
        class ResponseDecorator>
    void
    accept_ex(http::request<Body,
        http::basic_fields<Allocator>> const& req,
            ResponseDecorator const& decorator);

    /** Respond to a WebSocket HTTP Upgrade request

        This function is used to synchronously send the HTTP response
        to an HTTP request possibly containing a WebSocket Upgrade.
        The call blocks until one of the following conditions is true:

        @li The response finishes sending.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to
        the next layer's `read_some` and `write_some` functions.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When this call returns, the stream is then ready to send and
        receive WebSocket protocol frames and messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure.

        @param req An object containing the HTTP Upgrade request.
        Ownership is not transferred, the implementation will not
        access this object from other threads.

        @param ec Set to indicate what error occurred, if any.
    */
    template<class Body, class Allocator>
    void
    accept(http::request<Body,
        http::basic_fields<Allocator>> const& req,
            error_code& ec);

    /** Respond to a WebSocket HTTP Upgrade request

        This function is used to synchronously send the HTTP response
        to an HTTP request possibly containing a WebSocket Upgrade.
        The call blocks until one of the following conditions is true:

        @li The response finishes sending.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to
        the next layer's `read_some` and `write_some` functions.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When this call returns, the stream is then ready to send and
        receive WebSocket protocol frames and messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure.

        @param req An object containing the HTTP Upgrade request.
        Ownership is not transferred, the implementation will not
        access this object from other threads.

        @param decorator A function object which will be called to modify
        the HTTP response object delivered by the implementation. This
        could be used to set the Server field, subprotocols, or other
        application or HTTP specific fields. The object will be called
        with this equivalent signature:
        @code void decorator(
            response_type& res
        ); @endcode

        @param ec Set to indicate what error occurred, if any.
    */
    template<class Body, class Allocator,
        class ResponseDecorator>
    void
    accept_ex(http::request<Body,
        http::basic_fields<Allocator>> const& req,
            ResponseDecorator const& decorator,
                error_code& ec);

    /** Respond to a WebSocket HTTP Upgrade request

        This function is used to synchronously send the HTTP response
        to an HTTP request possibly containing a WebSocket Upgrade.
        The call blocks until one of the following conditions is true:

        @li The response finishes sending.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to
        the next layer's `read_some` and `write_some` functions.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When this call returns, the stream is then ready to send and
        receive WebSocket protocol frames and messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure.

        @param req An object containing the HTTP Upgrade request.
        Ownership is not transferred, the implementation will not
        access this object from other threads.

        @param buffers Caller provided data that has already been
        received on the stream. This must not include the octets
        corresponding to the HTTP Upgrade request. The implementation
        will copy the caller provided data before the function returns.

        @throws system_error Thrown on failure.
    */
    template<class Body, class Allocator,
        class ConstBufferSequence>
    void
    accept(http::request<Body,
        http::basic_fields<Allocator>> const& req,
            ConstBufferSequence const& buffers);

    /** Respond to a WebSocket HTTP Upgrade request

        This function is used to synchronously send the HTTP response
        to an HTTP request possibly containing a WebSocket Upgrade.
        The call blocks until one of the following conditions is true:

        @li The response finishes sending.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to
        the next layer's `read_some` and `write_some` functions.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When this call returns, the stream is then ready to send and
        receive WebSocket protocol frames and messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure.

        @param req An object containing the HTTP Upgrade request.
        Ownership is not transferred, the implementation will not
        access this object from other threads.

        @param buffers Caller provided data that has already been
        received on the stream. This must not include the octets
        corresponding to the HTTP Upgrade request. The implementation
        will copy the caller provided data before the function returns.

        @param decorator A function object which will be called to modify
        the HTTP response object delivered by the implementation. This
        could be used to set the Server field, subprotocols, or other
        application or HTTP specific fields. The object will be called
        with this equivalent signature:
        @code void decorator(
            response_type& res
        ); @endcode

        @throws system_error Thrown on failure.
    */
    template<class Body, class Allocator,
        class ConstBufferSequence, class ResponseDecorator>
    void
    accept_ex(http::request<Body,
        http::basic_fields<Allocator>> const& req,
            ConstBufferSequence const& buffers,
                ResponseDecorator const& decorator);

    /** Respond to a WebSocket HTTP Upgrade request

        This function is used to synchronously send the HTTP response
        to an HTTP request possibly containing a WebSocket Upgrade.
        The call blocks until one of the following conditions is true:

        @li The response finishes sending.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to
        the next layer's `read_some` and `write_some` functions.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When this call returns, the stream is then ready to send and
        receive WebSocket protocol frames and messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure.

        @param req An object containing the HTTP Upgrade request.
        Ownership is not transferred, the implementation will not
        access this object from other threads.

        @param buffers Caller provided data that has already been
        received on the stream. This must not include the octets
        corresponding to the HTTP Upgrade request. The implementation
        will copy the caller provided data before the function returns.

        @param ec Set to indicate what error occurred, if any.
    */
    template<class Body, class Allocator,
        class ConstBufferSequence>
    void
    accept(http::request<Body,
        http::basic_fields<Allocator>> const& req,
            ConstBufferSequence const& buffers,
                error_code& ec);

    /** Respond to a WebSocket HTTP Upgrade request

        This function is used to synchronously send the HTTP response
        to an HTTP request possibly containing a WebSocket Upgrade.
        The call blocks until one of the following conditions is true:

        @li The response finishes sending.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to
        the next layer's `read_some` and `write_some` functions.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When this call returns, the stream is then ready to send and
        receive WebSocket protocol frames and messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure.

        @param req An object containing the HTTP Upgrade request.
        Ownership is not transferred, the implementation will not
        access this object from other threads.

        @param buffers Caller provided data that has already been
        received on the stream. This must not include the octets
        corresponding to the HTTP Upgrade request. The implementation
        will copy the caller provided data before the function returns.

        @param decorator A function object which will be called to modify
        the HTTP response object delivered by the implementation. This
        could be used to set the Server field, subprotocols, or other
        application or HTTP specific fields. The object will be called
        with this equivalent signature:
        @code void decorator(
            response_type& res
        ); @endcode

        @param ec Set to indicate what error occurred, if any.
    */
    template<class Body, class Allocator,
        class ConstBufferSequence, class ResponseDecorator>
    void
    accept_ex(http::request<Body,
        http::basic_fields<Allocator>> const& req,
            ConstBufferSequence const& buffers,
                ResponseDecorator const& decorator,
                    error_code& ec);

    /** Start reading and responding to a WebSocket HTTP Upgrade request.

        This function is used to asynchronously read an HTTP WebSocket
        Upgrade request and send the HTTP response. The function call
        always returns immediately. The asynchronous operation will
        continue until one of the following conditions is true:

        @li The request is received and the response finishes sending.

        @li An error occurs on the stream.

        This operation is implemented in terms of one or more calls to
        the next layer's `async_read_some` and `async_write_some`
        functions, and is known as a <em>composed operation</em>. The
        program must ensure that the stream performs no other
        asynchronous operations until this operation completes.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When the completion handler is invoked, the stream is then
        ready to send and receive WebSocket protocol frames and
        messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure, and
        the completion handler will be invoked with a suitable error
        code set.

        @param handler The handler to be called when the request
        completes. Copies will be made of the handler as required. The
        equivalent function signature of the handler must be:
        @code void handler(
            error_code const& ec    // Result of operation
        ); @endcode
        Regardless of whether the asynchronous operation completes
        immediately or not, the handler will not be invoked from within
        this function. Invocation of the handler will be performed in a
        manner equivalent to using `boost::asio::io_service::post`.
    */
    template<class AcceptHandler>
#if BOOST_BEAST_DOXYGEN
    void_or_deduced
#else
    async_return_type<
        AcceptHandler, void(error_code)>
#endif
    async_accept(AcceptHandler&& handler);

    /** Start reading and responding to a WebSocket HTTP Upgrade request.

        This function is used to asynchronously read an HTTP WebSocket
        Upgrade request and send the HTTP response. The function call
        always returns immediately. The asynchronous operation will
        continue until one of the following conditions is true:

        @li The request is received and the response finishes sending.

        @li An error occurs on the stream.

        This operation is implemented in terms of one or more calls to
        the next layer's `async_read_some` and `async_write_some`
        functions, and is known as a <em>composed operation</em>. The
        program must ensure that the stream performs no other
        asynchronous operations until this operation completes.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When the completion handler is invoked, the stream is then
        ready to send and receive WebSocket protocol frames and
        messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure, and
        the completion handler will be invoked with a suitable error
        code set.

        @param decorator A function object which will be called to modify
        the HTTP response object delivered by the implementation. This
        could be used to set the Server field, subprotocols, or other
        application or HTTP specific fields. The object will be called
        with this equivalent signature:
        @code void decorator(
            response_type& res
        ); @endcode

        @param handler The handler to be called when the request
        completes. Copies will be made of the handler as required. The
        equivalent function signature of the handler must be:
        @code void handler(
            error_code const& ec    // Result of operation
        ); @endcode
        Regardless of whether the asynchronous operation completes
        immediately or not, the handler will not be invoked from within
        this function. Invocation of the handler will be performed in a
        manner equivalent to using `boost::asio::io_service::post`.
    */
    template<class ResponseDecorator, class AcceptHandler>
#if BOOST_BEAST_DOXYGEN
    void_or_deduced
#else
    async_return_type<
        AcceptHandler, void(error_code)>
#endif
    async_accept_ex(ResponseDecorator const& decorator,
        AcceptHandler&& handler);

    /** Start reading and responding to a WebSocket HTTP Upgrade request.

        This function is used to asynchronously read an HTTP WebSocket
        Upgrade request and send the HTTP response. The function call
        always returns immediately. The asynchronous operation will
        continue until one of the following conditions is true:

        @li The request is received and the response finishes sending.

        @li An error occurs on the stream.

        This operation is implemented in terms of one or more calls to
        the next layer's `async_read_some` and `async_write_some`
        functions, and is known as a <em>composed operation</em>. The
        program must ensure that the stream performs no other
        asynchronous operations until this operation completes.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When the completion handler is invoked, the stream is then
        ready to send and receive WebSocket protocol frames and
        messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure, and
        the completion handler will be invoked with a suitable error
        code set.

        @param buffers Caller provided data that has already been
        received on the stream. This may be used for implementations
        allowing multiple protocols on the same stream. The
        buffered data will first be applied to the handshake, and
        then to received WebSocket frames. The implementation will
        copy the caller provided data before the function returns.

        @param handler The handler to be called when the request
        completes. Copies will be made of the handler as required. The
        equivalent function signature of the handler must be:
        @code void handler(
            error_code const& ec    // Result of operation
        ); @endcode
        Regardless of whether the asynchronous operation completes
        immediately or not, the handler will not be invoked from within
        this function. Invocation of the handler will be performed in a
        manner equivalent to using `boost::asio::io_service::post`.
    */
    template<class ConstBufferSequence, class AcceptHandler>
#if BOOST_BEAST_DOXYGEN
    void_or_deduced
#else
    typename std::enable_if<
        ! http::detail::is_header<ConstBufferSequence>::value,
        async_return_type<AcceptHandler, void(error_code)>>::type
#endif
    async_accept(ConstBufferSequence const& buffers,
        AcceptHandler&& handler);

    /** Start reading and responding to a WebSocket HTTP Upgrade request.

        This function is used to asynchronously read an HTTP WebSocket
        Upgrade request and send the HTTP response. The function call
        always returns immediately. The asynchronous operation will
        continue until one of the following conditions is true:

        @li The request is received and the response finishes sending.

        @li An error occurs on the stream.

        This operation is implemented in terms of one or more calls to
        the next layer's `async_read_some` and `async_write_some`
        functions, and is known as a <em>composed operation</em>. The
        program must ensure that the stream performs no other
        asynchronous operations until this operation completes.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When the completion handler is invoked, the stream is then
        ready to send and receive WebSocket protocol frames and
        messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure, and
        the completion handler will be invoked with a suitable error
        code set.

        @param buffers Caller provided data that has already been
        received on the stream. This may be used for implementations
        allowing multiple protocols on the same stream. The
        buffered data will first be applied to the handshake, and
        then to received WebSocket frames. The implementation will
        copy the caller provided data before the function returns.

        @param decorator A function object which will be called to modify
        the HTTP response object delivered by the implementation. This
        could be used to set the Server field, subprotocols, or other
        application or HTTP specific fields. The object will be called
        with this equivalent signature:
        @code void decorator(
            response_type& res
        ); @endcode

        @param handler The handler to be called when the request
        completes. Copies will be made of the handler as required. The
        equivalent function signature of the handler must be:
        @code void handler(
            error_code const& ec    // Result of operation
        ); @endcode
        Regardless of whether the asynchronous operation completes
        immediately or not, the handler will not be invoked from within
        this function. Invocation of the handler will be performed in a
        manner equivalent to using `boost::asio::io_service::post`.
    */
    template<class ConstBufferSequence,
        class ResponseDecorator, class AcceptHandler>
#if BOOST_BEAST_DOXYGEN
    void_or_deduced
#else
    typename std::enable_if<
        ! http::detail::is_header<ConstBufferSequence>::value,
        async_return_type<AcceptHandler, void(error_code)>>::type
#endif
    async_accept_ex(ConstBufferSequence const& buffers,
        ResponseDecorator const& decorator,
            AcceptHandler&& handler);

    /** Start responding to a WebSocket HTTP Upgrade request.

        This function is used to asynchronously send the HTTP response
        to an HTTP request possibly containing a WebSocket Upgrade
        request. The function call always returns immediately. The
        asynchronous operation will continue until one of the following
        conditions is true:

        @li The response finishes sending.

        @li An error occurs on the stream.

        This operation is implemented in terms of one or more calls to
        the next layer's `async_write_some` functions, and is known as
        a <em>composed operation</em>. The program must ensure that the
        stream performs no other operations until this operation
        completes.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When the completion handler is invoked, the stream is then
        ready to send and receive WebSocket protocol frames and
        messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure, and
        the completion handler will be invoked with a suitable error
        code set.

        @param req An object containing the HTTP Upgrade request.
        Ownership is not transferred, the implementation will not access
        this object from other threads.

        @param handler The handler to be called when the request
        completes. Copies will be made of the handler as required. The
        equivalent function signature of the handler must be:
        @code void handler(
            error_code const& ec    // Result of operation
        ); @endcode
        Regardless of whether the asynchronous operation completes
        immediately or not, the handler will not be invoked from within
        this function. Invocation of the handler will be performed in a
        manner equivalent to using `boost::asio::io_service::post`.
    */
    template<class Body, class Allocator,
        class AcceptHandler>
#if BOOST_BEAST_DOXYGEN
    void_or_deduced
#else
    async_return_type<
        AcceptHandler, void(error_code)>
#endif
    async_accept(http::request<Body,
        http::basic_fields<Allocator>> const& req,
            AcceptHandler&& handler);

    /** Start responding to a WebSocket HTTP Upgrade request.

        This function is used to asynchronously send the HTTP response
        to an HTTP request possibly containing a WebSocket Upgrade
        request. The function call always returns immediately. The
        asynchronous operation will continue until one of the following
        conditions is true:

        @li The response finishes sending.

        @li An error occurs on the stream.

        This operation is implemented in terms of one or more calls to
        the next layer's `async_write_some` functions, and is known as
        a <em>composed operation</em>. The program must ensure that the
        stream performs no other operations until this operation
        completes.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When the completion handler is invoked, the stream is then
        ready to send and receive WebSocket protocol frames and
        messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure, and
        the completion handler will be invoked with a suitable error
        code set.

        @param req An object containing the HTTP Upgrade request.
        Ownership is not transferred, the implementation will not access
        this object from other threads.

        @param decorator A function object which will be called to modify
        the HTTP response object delivered by the implementation. This
        could be used to set the Server field, subprotocols, or other
        application or HTTP specific fields. The object will be called
        with this equivalent signature:
        @code void decorator(
            response_type& res
        ); @endcode

        @param handler The handler to be called when the request
        completes. Copies will be made of the handler as required. The
        equivalent function signature of the handler must be:
        @code void handler(
            error_code const& ec    // Result of operation
        ); @endcode
        Regardless of whether the asynchronous operation completes
        immediately or not, the handler will not be invoked from within
        this function. Invocation of the handler will be performed in a
        manner equivalent to using `boost::asio::io_service::post`.
    */
    template<class Body, class Allocator,
        class ResponseDecorator, class AcceptHandler>
#if BOOST_BEAST_DOXYGEN
    void_or_deduced
#else
    async_return_type<
        AcceptHandler, void(error_code)>
#endif
    async_accept_ex(http::request<Body,
        http::basic_fields<Allocator>> const& req,
            ResponseDecorator const& decorator,
                AcceptHandler&& handler);

    /** Start responding to a WebSocket HTTP Upgrade request.

        This function is used to asynchronously send the HTTP response
        to an HTTP request possibly containing a WebSocket Upgrade
        request. The function call always returns immediately. The
        asynchronous operation will continue until one of the following
        conditions is true:

        @li The response finishes sending.

        @li An error occurs on the stream.

        This operation is implemented in terms of one or more calls to
        the next layer's `async_write_some` functions, and is known as
        a <em>composed operation</em>. The program must ensure that the
        stream performs no other operations until this operation
        completes.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When the completion handler is invoked, the stream is then
        ready to send and receive WebSocket protocol frames and
        messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure, and
        the completion handler will be invoked with a suitable error
        code set.

        @param req An object containing the HTTP Upgrade request.
        Ownership is not transferred, the implementation will not access
        this object from other threads.

        @param buffers Caller provided data that has already been
        received on the stream. This may be used for implementations
        allowing multiple protocols on the same stream. The
        buffered data will first be applied to the handshake, and
        then to received WebSocket frames. The implementation will
        copy the caller provided data before the function returns.

        @param handler The handler to be called when the request
        completes. Copies will be made of the handler as required. The
        equivalent function signature of the handler must be:
        @code void handler(
            error_code const& ec    // Result of operation
        ); @endcode
        Regardless of whether the asynchronous operation completes
        immediately or not, the handler will not be invoked from within
        this function. Invocation of the handler will be performed in a
        manner equivalent to using `boost::asio::io_service::post`.
    */
    template<class Body, class Allocator,
        class ConstBufferSequence, class AcceptHandler>
#if BOOST_BEAST_DOXYGEN
    void_or_deduced
#else
    async_return_type<
        AcceptHandler, void(error_code)>
#endif
    async_accept(http::request<Body,
        http::basic_fields<Allocator>> const& req,
            ConstBufferSequence const& buffers,
                AcceptHandler&& handler);

    /** Start responding to a WebSocket HTTP Upgrade request.

        This function is used to asynchronously send the HTTP response
        to an HTTP request possibly containing a WebSocket Upgrade
        request. The function call always returns immediately. The
        asynchronous operation will continue until one of the following
        conditions is true:

        @li The response finishes sending.

        @li An error occurs on the stream.

        This operation is implemented in terms of one or more calls to
        the next layer's `async_write_some` functions, and is known as
        a <em>composed operation</em>. The program must ensure that the
        stream performs no other operations until this operation
        completes.

        If the stream receives a valid HTTP WebSocket Upgrade request,
        an HTTP response is sent back indicating a successful upgrade.
        When the completion handler is invoked, the stream is then
        ready to send and receive WebSocket protocol frames and
        messages.

        If the HTTP Upgrade request is invalid or cannot be satisfied,
        an HTTP response is sent indicating the reason and status code
        (typically 400, "Bad Request"). This counts as a failure, and
        the completion handler will be invoked with a suitable error
        code set.

        @param req An object containing the HTTP Upgrade request.
        Ownership is not transferred, the implementation will not access
        this object from other threads.

        @param buffers Caller provided data that has already been
        received on the stream. This may be used for implementations
        allowing multiple protocols on the same stream. The
        buffered data will first be applied to the handshake, and
        then to received WebSocket frames. The implementation will
        copy the caller provided data before the function returns.

        @param decorator A function object which will be called to modify
        the HTTP response object delivered by the implementation. This
        could be used to set the Server field, subprotocols, or other
        application or HTTP specific fields. The object will be called
        with this equivalent signature:
        @code void decorator(
            response_type& res
        ); @endcode

        @param handler The handler to be called when the request
        completes. Copies will be made of the handler as required. The
        equivalent function signature of the handler must be:
        @code void handler(
            error_code const& ec    // Result of operation
        ); @endcode
        Regardless of whether the asynchronous operation completes
        immediately or not, the handler will not be invoked from within
        this function. Invocation of the handler will be performed in a
        manner equivalent to using `boost::asio::io_service::post`.
    */
    template<
        class Body, class Allocator,
        class ConstBufferSequence,
        class ResponseDecorator,
        class AcceptHandler>
#if BOOST_BEAST_DOXYGEN
    void_or_deduced
#else
    async_return_type<
        AcceptHandler, void(error_code)>
#endif
    async_accept_ex(
        http::request<Body,
            http::basic_fields<Allocator>> const& req,
        ConstBufferSequence const& buffers,
        ResponseDecorator const& decorator,
        AcceptHandler&& handler);

    /** Send an HTTP WebSocket Upgrade request and receive the response.

        This function is used to synchronously send the WebSocket
        upgrade HTTP request. The call blocks until one of the
        following conditions is true:

        @li The request is sent and the response is received.

        @li An error occurs on the stream

        This function is implemented in terms of one or more calls to the
        next layer's `read_some` and `write_some` functions.

        The operation is successful if the received HTTP response indicates
        a successful HTTP Upgrade (represented by a Status-Code of 101,
        "switching protocols").

        @param host The name of the remote host,
        required by the HTTP protocol.

        @param target The Request Target, which may not be empty,
        required by the HTTP protocol.

        @throws system_error Thrown on failure.

        @par Example
        @code
        websocket::stream<ip::tcp::socket> ws{io_service};
        ...
        try
        {
            ws.handshake("localhost", "/");
        }
        catch(...)
        {
            // An error occurred.
        }
        @endcode
    */
    void
    handshake(
        string_view host,
        string_view target);

    /** Send an HTTP WebSocket Upgrade request and receive the response.

        This function is used to synchronously send the WebSocket
        upgrade HTTP request. The call blocks until one of the
        following conditions is true:

        @li The request is sent and the response is received.

        @li An error occurs on the stream

        This function is implemented in terms of one or more calls to the
        next layer's `read_some` and `write_some` functions.

        The operation is successful if the received HTTP response indicates
        a successful HTTP Upgrade (represented by a Status-Code of 101,
        "switching protocols").

        @param res The HTTP Upgrade response returned by the remote
        endpoint.

        @param host The name of the remote host,
        required by the HTTP protocol.

        @param target The Request Target, which may not be empty,
        required by the HTTP protocol.

        @throws system_error Thrown on failure.

        @par Example
        @code
        websocket::stream<ip::tcp::socket> ws{io_service};
        ...
        try
        {
            response_type res;
            ws.handshake(res, "localhost", "/");
        }
        catch(...)
        {
            // An error occurred.
        }
        @endcode
    */
    void
    handshake(
        response_type& res,
        string_view host,
        string_view target);

    /** Send an HTTP WebSocket Upgrade request and receive the response.

        This function is used to synchronously send the WebSocket
        upgrade HTTP request. The call blocks until one of the
        following conditions is true:

        @li The request is sent and the response is received.

        @li An error occurs on the stream

        This function is implemented in terms of one or more calls to the
        next layer's `read_some` and `write_some` functions.

        The operation is successful if the received HTTP response indicates
        a successful HTTP Upgrade (represented by a Status-Code of 101,
        "switching protocols").

        @param host The name of the remote host,
        required by the HTTP protocol.

        @param target The Request Target, which may not be empty,
        required by the HTTP protocol.

        @param decorator A function object which will be called to modify
        the HTTP request object generated by the implementation. This
        could be used to set the User-Agent field, subprotocols, or other
        application or HTTP specific fields. The object will be called
        with this equivalent signature:
        @code void decorator(
            request_type& req
        ); @endcode

        @throws system_error Thrown on failure.

        @par Example
        @code
        websocket::stream<ip::tcp::socket> ws{io_service};
        ...
        try
        {
            ws.handshake("localhost", "/",
                [](request_type& req)
                {
                    req.set(field::user_agent, "Beast");
                });
        }
        catch(...)
        {
            // An error occurred.
        }
        @endcode
    */
    template<class RequestDecorator>
    void
    handshake_ex(
        string_view host,
        string_view target,
        RequestDecorator const& decorator);

    /** Send an HTTP WebSocket Upgrade request and receive the response.

        This function is used to synchronously send the WebSocket
        upgrade HTTP request. The call blocks until one of the
        following conditions is true:

        @li The request is sent and the response is received.

        @li An error occurs on the stream

        This function is implemented in terms of one or more calls to the
        next layer's `read_some` and `write_some` functions.

        The operation is successful if the received HTTP response indicates
        a successful HTTP Upgrade (represented by a Status-Code of 101,
        "switching protocols").

        @param res The HTTP Upgrade response returned by the remote
        endpoint.

        @param host The name of the remote host,
        required by the HTTP protocol.

        @param target The Request Target, which may not be empty,
        required by the HTTP protocol.

        @param decorator A function object which will be called to modify
        the HTTP request object generated by the implementation. This
        could be used to set the User-Agent field, subprotocols, or other
        application or HTTP specific fields. The object will be called
        with this equivalent signature:
        @code void decorator(
            request_type& req
        ); @endcode

        @throws system_error Thrown on failure.

        @par Example
        @code
        websocket::stream<ip::tcp::socket> ws{io_service};
        ...
        try
        {
            response_type res;
            ws.handshake(res, "localhost", "/",
                [](request_type& req)
                {
                    req.set(field::user_agent, "Beast");
                });
        }
        catch(...)
        {
            // An error occurred.
        }
        @endcode
    */
    template<class RequestDecorator>
    void
    handshake_ex(
        response_type& res,
        string_view host,
        string_view target,
        RequestDecorator const& decorator);

    /** Send an HTTP WebSocket Upgrade request and receive the response.

        This function is used to synchronously send the WebSocket
        upgrade HTTP request. The call blocks until one of the
        following conditions is true:

        @li The request is sent and the response is received.

        @li An error occurs on the stream

        This function is implemented in terms of one or more calls to the
        next layer's `read_some` and `write_some` functions.

        The operation is successful if the received HTTP response indicates
        a successful HTTP Upgrade (represented by a Status-Code of 101,
        "switching protocols").

        @param host The name of the remote host,
        required by the HTTP protocol.

        @param target The Request Target, which may not be empty,
        required by the HTTP protocol.

        @param ec Set to indicate what error occurred, if any.

        @par Example
        @code
        websocket::stream<ip::tcp::socket> ws{io_service};
        ...
        error_code ec;
        ws.handshake(host, target, ec);
        if(ec)
        {
            // An error occurred.
        }
        @endcode
    */
    void
    handshake(
        string_view host,
        string_view target,
        error_code& ec);

    /** Send an HTTP WebSocket Upgrade request and receive the response.

        This function is used to synchronously send the WebSocket
        upgrade HTTP request. The call blocks until one of the
        following conditions is true:

        @li The request is sent and the response is received.

        @li An error occurs on the stream

        This function is implemented in terms of one or more calls to the
        next layer's `read_some` and `write_some` functions.

        The operation is successful if the received HTTP response indicates
        a successful HTTP Upgrade (represented by a Status-Code of 101,
        "switching protocols").

        @param res The HTTP Upgrade response returned by the remote
        endpoint. If `ec` is set, the returned value is undefined.

        @param host The name of the remote host,
        required by the HTTP protocol.

        @param target The Request Target, which may not be empty,
        required by the HTTP protocol.

        @param ec Set to indicate what error occurred, if any.

        @par Example
        @code
        websocket::stream<ip::tcp::socket> ws{io_service};
        ...
        error_code ec;
        response_type res;
        ws.handshake(res, host, target, ec);
        if(ec)
        {
            // An error occurred.
        }
        @endcode
    */
    void
    handshake(
        response_type& res,
        string_view host,
        string_view target,
        error_code& ec);

    /** Send an HTTP WebSocket Upgrade request and receive the response.

        This function is used to synchronously send the WebSocket
        upgrade HTTP request. The call blocks until one of the
        following conditions is true:

        @li The request is sent and the response is received.

        @li An error occurs on the stream

        This function is implemented in terms of one or more calls to the
        next layer's `read_some` and `write_some` functions.

        The operation is successful if the received HTTP response indicates
        a successful HTTP Upgrade (represented by a Status-Code of 101,
        "switching protocols").

        @param host The name of the remote host,
        required by the HTTP protocol.

        @param target The Request Target, which may not be empty,
        required by the HTTP protocol.

        @param decorator A function object which will be called to modify
        the HTTP request object generated by the implementation. This
        could be used to set the User-Agent field, subprotocols, or other
        application or HTTP specific fields. The object will be called
        with this equivalent signature:
        @code void decorator(
            request_type& req
        ); @endcode

        @param ec Set to indicate what error occurred, if any.

        @par Example
        @code
        websocket::stream<ip::tcp::socket> ws{io_service};
        ...
        error_code ec;
        ws.handshake("localhost", "/",
            [](request_type& req)
            {
                req.set(field::user_agent, "Beast");
            },
            ec);
        if(ec)
        {
            // An error occurred.
        }
        @endcode
    */
    template<class RequestDecorator>
    void
    handshake_ex(
        string_view host,
        string_view target,
        RequestDecorator const& decorator,
        error_code& ec);

    /** Send an HTTP WebSocket Upgrade request and receive the response.

        This function is used to synchronously send the WebSocket
        upgrade HTTP request. The call blocks until one of the
        following conditions is true:

        @li The request is sent and the response is received.

        @li An error occurs on the stream

        This function is implemented in terms of one or more calls to the
        next layer's `read_some` and `write_some` functions.

        The operation is successful if the received HTTP response indicates
        a successful HTTP Upgrade (represented by a Status-Code of 101,
        "switching protocols").

        @param res The HTTP Upgrade response returned by the remote
        endpoint.

        @param host The name of the remote host,
        required by the HTTP protocol.

        @param target The Request Target, which may not be empty,
        required by the HTTP protocol.

        @param decorator A function object which will be called to modify
        the HTTP request object generated by the implementation. This
        could be used to set the User-Agent field, subprotocols, or other
        application or HTTP specific fields. The object will be called
        with this equivalent signature:
        @code void decorator(
            request_type& req
        ); @endcode

        @param ec Set to indicate what error occurred, if any.

        @par Example
        @code
        websocket::stream<ip::tcp::socket> ws{io_service};
        ...
        error_code ec;
        response_type res;
        ws.handshake(res, "localhost", "/",
            [](request_type& req)
            {
                req.set(field::user_agent, "Beast");
            },
            ec);
        if(ec)
        {
            // An error occurred.
        }
        @endcode
    */
    template<class RequestDecorator>
    void
    handshake_ex(
        response_type& res,
        string_view host,
        string_view target,
        RequestDecorator const& decorator,
        error_code& ec);

    /** Start an asynchronous operation to send an upgrade request and receive the response.

        This function is used to asynchronously send the HTTP WebSocket
        upgrade request and receive the HTTP WebSocket Upgrade response.
        This function call always returns immediately. The asynchronous
        operation will continue until one of the following conditions is
        true:

        @li The request is sent and the response is received.

        @li An error occurs on the stream

        This operation is implemented in terms of one or more calls to the
        next layer's `async_read_some` and `async_write_some` functions, and
        is known as a <em>composed operation</em>. The program must ensure
        that the stream performs no other operations until this operation
        completes.

        The operation is successful if the received HTTP response indicates
        a successful HTTP Upgrade (represented by a Status-Code of 101,
        "switching protocols").

        @param host The name of the remote host, required by
        the HTTP protocol. Copies may be made as needed.

        @param target The Request Target, which may not be empty,
        required by the HTTP protocol. Copies of this parameter may
        be made as needed.

        @param handler The handler to be called when the request completes.
        Copies will be made of the handler as required. The equivalent
        function signature of the handler must be:
        @code void handler(
            error_code const& ec    // Result of operation
        ); @endcode
        Regardless of whether the asynchronous operation completes
        immediately or not, the handler will not be invoked from within
        this function. Invocation of the handler will be performed in a
        manner equivalent to using `boost::asio::io_service::post`.
    */
    template<class HandshakeHandler>
#if BOOST_BEAST_DOXYGEN
    void_or_deduced
#else
    async_return_type<
        HandshakeHandler, void(error_code)>
#endif
    async_handshake(
        string_view host,
        string_view target,
        HandshakeHandler&& handler);

    /** Start an asynchronous operation to send an upgrade request and receive the response.

        This function is used to asynchronously send the HTTP WebSocket
        upgrade request and receive the HTTP WebSocket Upgrade response.
        This function call always returns immediately. The asynchronous
        operation will continue until one of the following conditions is
        true:

        @li The request is sent and the response is received.

        @li An error occurs on the stream

        This operation is implemented in terms of one or more calls to the
        next layer's `async_read_some` and `async_write_some` functions, and
        is known as a <em>composed operation</em>. The program must ensure
        that the stream performs no other operations until this operation
        completes.

        The operation is successful if the received HTTP response indicates
        a successful HTTP Upgrade (represented by a Status-Code of 101,
        "switching protocols").

        @param res The HTTP Upgrade response returned by the remote
        endpoint. The caller must ensure this object is valid for at
        least until the completion handler is invoked.

        @param host The name of the remote host, required by
        the HTTP protocol. Copies may be made as needed.

        @param target The Request Target, which may not be empty,
        required by the HTTP protocol. Copies of this parameter may
        be made as needed.

        @param handler The handler to be called when the request completes.
        Copies will be made of the handler as required. The equivalent
        function signature of the handler must be:
        @code void handler(
            error_code const& ec     // Result of operation
        ); @endcode
        Regardless of whether the asynchronous operation completes
        immediately or not, the handler will not be invoked from within
        this function. Invocation of the handler will be performed in a
        manner equivalent to using `boost::asio::io_service::post`.
    */
    template<class HandshakeHandler>
#if BOOST_BEAST_DOXYGEN
    void_or_deduced
#else
    async_return_type<
        HandshakeHandler, void(error_code)>
#endif
    async_handshake(
        response_type& res,
        string_view host,
        string_view target,
        HandshakeHandler&& handler);

    /** Start an asynchronous operation to send an upgrade request and receive the response.

        This function is used to asynchronously send the HTTP WebSocket
        upgrade request and receive the HTTP WebSocket Upgrade response.
        This function call always returns immediately. The asynchronous
        operation will continue until one of the following conditions is
        true:

        @li The request is sent and the response is received.

        @li An error occurs on the stream

        This operation is implemented in terms of one or more calls to the
        next layer's `async_read_some` and `async_write_some` functions, and
        is known as a <em>composed operation</em>. The program must ensure
        that the stream performs no other operations until this operation
        completes.

        The operation is successful if the received HTTP response indicates
        a successful HTTP Upgrade (represented by a Status-Code of 101,
        "switching protocols").

        @param host The name of the remote host, required by
        the HTTP protocol. Copies may be made as needed.

        @param target The Request Target, which may not be empty,
        required by the HTTP protocol. Copies of this parameter may
        be made as needed.

        @param decorator A function object which will be called to modify
        the HTTP request object generated by the implementation. This
        could be used to set the User-Agent field, subprotocols, or other
        application or HTTP specific fields. The object will be called
        with this equivalent signature:
        @code void decorator(
            request_type& req
        ); @endcode

        @param handler The handler to be called when the request completes.
        Copies will be made of the handler as required. The equivalent
        function signature of the handler must be:
        @code void handler(
            error_code const& ec     // Result of operation
        ); @endcode
        Regardless of whether the asynchronous operation completes
        immediately or not, the handler will not be invoked from within
        this function. Invocation of the handler will be performed in a
        manner equivalent to using `boost::asio::io_service::post`.
    */
    template<class RequestDecorator, class HandshakeHandler>
#if BOOST_BEAST_DOXYGEN
    void_or_deduced
#else
    async_return_type<
        HandshakeHandler, void(error_code)>
#endif
    async_handshake_ex(
        string_view host,
        string_view target,
        RequestDecorator const& decorator,
        HandshakeHandler&& handler);

    /** Start an asynchronous operation to send an upgrade request and receive the response.

        This function is used to asynchronously send the HTTP WebSocket
        upgrade request and receive the HTTP WebSocket Upgrade response.
        This function call always returns immediately. The asynchronous
        operation will continue until one of the following conditions is
        true:

        @li The request is sent and the response is received.

        @li An error occurs on the stream

        This operation is implemented in terms of one or more calls to the
        next layer's `async_read_some` and `async_write_some` functions, and
        is known as a <em>composed operation</em>. The program must ensure
        that the stream performs no other operations until this operation
        completes.

        The operation is successful if the received HTTP response indicates
        a successful HTTP Upgrade (represented by a Status-Code of 101,
        "switching protocols").

        @param res The HTTP Upgrade response returned by the remote
        endpoint. The caller must ensure this object is valid for at
        least until the completion handler is invoked.

        @param host The name of the remote host, required by
        the HTTP protocol. Copies may be made as needed.

        @param target The Request Target, which may not be empty,
        required by the HTTP protocol. Copies of this parameter may
        be made as needed.

        @param decorator A function object which will be called to modify
        the HTTP request object generated by the implementation. This
        could be used to set the User-Agent field, subprotocols, or other
        application or HTTP specific fields. The object will be called
        with this equivalent signature:
        @code void decorator(
            request_type& req
        ); @endcode

        @param handler The handler to be called when the request completes.
        Copies will be made of the handler as required. The equivalent
        function signature of the handler must be:
        @code void handler(
            error_code const& ec     // Result of operation
        ); @endcode
        Regardless of whether the asynchronous operation completes
        immediately or not, the handler will not be invoked from within
        this function. Invocation of the handler will be performed in a
        manner equivalent to using `boost::asio::io_service::post`.
    */
    template<class RequestDecorator, class HandshakeHandler>
#if BOOST_BEAST_DOXYGEN
    void_or_deduced
#else
    async_return_type<
        HandshakeHandler, void(error_code)>
#endif
    async_handshake_ex(
        response_type& res,
        string_view host,
        string_view target,
        RequestDecorator const& decorator,
        HandshakeHandler&& handler);

    //--------------------------------------------------------------------------
    //
    // Control Frames
    //
    //--------------------------------------------------------------------------

    /** Send a WebSocket close frame.

        This function is used to synchronously send a close frame on
        the stream. The call blocks until one of the following is true:

        @li The close frame finishes sending.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls
        to the next layer's `write_some` functions.

        If the close reason specifies a close code other than
        @ref beast::websocket::close_code::none, the close frame is
        sent with the close code and optional reason string. Otherwise,
        the close frame is sent with no payload.

        Callers should not attempt to write WebSocket data after
        initiating the close. Instead, callers should continue
        reading until an error occurs. A read returning @ref error::closed
        indicates a successful connection closure.

        @param cr The reason for the close.

        @throws system_error Thrown on failure.
    */
    void
    close(close_reason const& cr);

    /** Send a WebSocket close frame.

        This function is used to synchronously send a close frame on
        the stream. The call blocks until one of the following is true:

        @li The close frame finishes sending.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls
        to the next layer's `write_some` functions.

        If the close reason specifies a close code other than
        @ref beast::websocket::close_code::none, the close frame is
        sent with the close code and optional reason string. Otherwise,
        the close frame is sent with no payload.

        Callers should not attempt to write WebSocket data after
        initiating the close. Instead, callers should continue
        reading until an error occurs. A read returning @ref error::closed
        indicates a successful connection closure.

        @param cr The reason for the close.

        @param ec Set to indicate what error occurred, if any.
    */
    void
    close(close_reason const& cr, error_code& ec);

    /** Start an asynchronous operation to send a WebSocket close frame.

        This function is used to asynchronously send a close frame on
        the stream. This function call always returns immediately. The
        asynchronous operation will continue until one of the following
        conditions is true:

        @li The close frame finishes sending.

        @li An error occurs on the stream.

        This operation is implemented in terms of one or more calls to the
        next layer's `async_write_some` functions, and is known as a
        <em>composed operation</em>. The program must ensure that the
        stream performs no other write operations (such as @ref async_ping,
        @ref stream::async_write, @ref stream::async_write_some, or
        @ref stream::async_close) until this operation completes.

        If the close reason specifies a close code other than
        @ref beast::websocket::close_code::none, the close frame is
        sent with the close code and optional reason string. Otherwise,
        the close frame is sent with no payload.

        Callers should not attempt to write WebSocket data after
        initiating the close. Instead, callers should continue
        reading until an error occurs. A read returning @ref error::closed
        indicates a successful connection closure.

        @param cr The reason for the close.

        @param handler The handler to be called when the close operation
        completes. Copies will be made of the handler as required. The
        function signature of the handler must be:
        @code
        void handler(
            error_code const& ec     // Result of operation
        );
        @endcode
        Regardless of whether the asynchronous operation completes
        immediately or not, the handler will not be invoked from within
        this function. Invocation of the handler will be performed in a
        manner equivalent to using `boost::asio::io_service::post`.
    */
    template<class CloseHandler>
#if BOOST_BEAST_DOXYGEN
    void_or_deduced
#else
    async_return_type<
        CloseHandler, void(error_code)>
#endif
    async_close(close_reason const& cr, CloseHandler&& handler);

    /** Send a WebSocket ping frame.

        This function is used to synchronously send a ping frame on
        the stream. The call blocks until one of the following is true:

        @li The ping frame finishes sending.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to the
        next layer's `write_some` functions.

        @param payload The payload of the ping message, which may be empty.

        @throws system_error Thrown on failure.
    */
    void
    ping(ping_data const& payload);

    /** Send a WebSocket ping frame.

        This function is used to synchronously send a ping frame on
        the stream. The call blocks until one of the following is true:

        @li The ping frame finishes sending.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to the
        next layer's `write_some` functions.

        @param payload The payload of the ping message, which may be empty.

        @param ec Set to indicate what error occurred, if any.
    */
    void
    ping(ping_data const& payload, error_code& ec);

    /** Start an asynchronous operation to send a WebSocket ping frame.

        This function is used to asynchronously send a ping frame to
        the stream. The function call always returns immediately. The
        asynchronous operation will continue until one of the following
        is true:

        @li The entire ping frame is sent.

        @li An error occurs on the stream.

        This operation is implemented in terms of one or more calls to the
        next layer's `async_write_some` functions, and is known as a
        <em>composed operation</em>. The program must ensure that the
        stream performs no other writes until this operation completes.

        If a close frame is sent or received before the ping frame is
        sent, the completion handler will be called with the error
        set to `boost::asio::error::operation_aborted`.

        @param payload The payload of the ping message, which may be empty.

        @param handler The handler to be called when the read operation
        completes. Copies will be made of the handler as required. The
        function signature of the handler must be:
        @code
        void handler(
            error_code const& ec     // Result of operation
        );
        @endcode
        Regardless of whether the asynchronous operation completes
        immediately or not, the handler will not be invoked from within
        this function. Invocation of the handler will be performed in a
        manner equivalent to using `boost::asio::io_service::post`.
    */
    template<class WriteHandler>
#if BOOST_BEAST_DOXYGEN
    void_or_deduced
#else
    async_return_type<
        WriteHandler, void(error_code)>
#endif
    async_ping(ping_data const& payload, WriteHandler&& handler);

    /** Send a WebSocket pong frame.

        This function is used to synchronously send a pong frame on
        the stream. The call blocks until one of the following is true:

        @li The pong frame finishes sending.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to the
        next layer's `write_some` functions.

        The WebSocket protocol allows pong frames to be sent from either
        end at any time. It is not necessary to first receive a ping in
        order to send a pong. The remote peer may use the receipt of a
        pong frame as an indication that the connection is not dead.

        @param payload The payload of the pong message, which may be empty.

        @throws system_error Thrown on failure.
    */
    void
    pong(ping_data const& payload);

    /** Send a WebSocket pong frame.

        This function is used to synchronously send a pong frame on
        the stream. The call blocks until one of the following is true:

        @li The pong frame finishes sending.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to the
        next layer's `write_some` functions.

        The WebSocket protocol allows pong frames to be sent from either
        end at any time. It is not necessary to first receive a ping in
        order to send a pong. The remote peer may use the receipt of a
        pong frame as an indication that the connection is not dead.

        @param payload The payload of the pong message, which may be empty.

        @param ec Set to indicate what error occurred, if any.
    */
    void
    pong(ping_data const& payload, error_code& ec);

    /** Start an asynchronous operation to send a WebSocket pong frame.

        This function is used to asynchronously send a pong frame to
        the stream. The function call always returns immediately. The
        asynchronous operation will continue until one of the following
        is true:

        @li The entire pong frame is sent.

        @li An error occurs on the stream.

        This operation is implemented in terms of one or more calls to the
        next layer's `async_write_some` functions, and is known as a
        <em>composed operation</em>. The program must ensure that the
        stream performs no other writes until this operation completes.

        The WebSocket protocol allows pong frames to be sent from either
        end at any time. It is not necessary to first receive a ping in
        order to send a pong. The remote peer may use the receipt of a
        pong frame as an indication that the connection is not dead.

        If a close frame is sent or received before the pong frame is
        sent, the completion handler will be called with the error
        set to `boost::asio::error::operation_aborted`.

        @param payload The payload of the pong message, which may be empty.

        @param handler The handler to be called when the read operation
        completes. Copies will be made of the handler as required. The
        function signature of the handler must be:
        @code
        void handler(
            error_code const& ec     // Result of operation
        );
        @endcode
        Regardless of whether the asynchronous operation completes
        immediately or not, the handler will not be invoked from within
        this function. Invocation of the handler will be performed in a
        manner equivalent to using `boost::asio::io_service::post`.
    */
    template<class WriteHandler>
#if BOOST_BEAST_DOXYGEN
    void_or_deduced
#else
    async_return_type<
        WriteHandler, void(error_code)>
#endif
    async_pong(ping_data const& payload, WriteHandler&& handler);

    //--------------------------------------------------------------------------
    //
    // Reading
    //
    //--------------------------------------------------------------------------

    /** Start an asynchronous operation to read a message frame from the stream.

        This function is used to asynchronously read a single message
        frame from the websocket. The function call always returns
        immediately. The asynchronous operation will continue until
        one of the following conditions is true:

        @li A complete frame is received.

        @li An error occurs on the stream.

        This operation is implemented in terms of one or more calls to the
        next layer's `async_read_some` and `async_write_some` functions,
        and is known as a <em>composed operation</em>. The program must
        ensure that the stream performs no other reads until this operation
        completes.

        During reads, the implementation handles control frames as
        follows:

        @li The @ref control_callback is invoked when a ping frame
            or pong frame is received.

        @li A pong frame is sent when a ping frame is received.

        @li The WebSocket close procedure is started if a close frame
            is received. In this case, the operation will eventually
            complete with the error set to @ref error::closed.

        Because of the need to handle control frames, read operations
        can cause writes to take place. These writes are managed
        transparently; callers can still have one active asynchronous
        read and asynchronous write operation pending simultaneously
        (a user initiated call to @ref async_close counts as a write).

        @param buffer A dynamic buffer to hold the message data after
        any masking or decompression has been applied. This object must
        remain valid until the handler is called.

        @param handler The handler to be called when the read operation
        completes. Copies will be made of the handler as required. The
        function signature of the handler must be:
        @code
        void handler(
            error_code const& ec,   // Result of operation
            bool fin                // `true` if this is the last frame
        );
        @endcode
        Regardless of whether the asynchronous operation completes
        immediately or not, the handler will not be invoked from within
        this function. Invocation of the handler will be performed in a
        manner equivalent to using boost::asio::io_service::post().
    */
    template<class DynamicBuffer, class ReadHandler>
#if BOOST_BEAST_DOXYGEN
    void_or_deduced
#else
    async_return_type<ReadHandler, void(error_code, bool)>
#endif
    async_read_frame(DynamicBuffer& buffer, ReadHandler&& handler);

    //--------------------------------------------------------------------------

    /** Read a message from the stream.

        This function is used to synchronously read a message from
        the stream. The call blocks until one of the following is true:

        @li A complete message is received.

        @li An error occurs on the stream.

        This call is implemented in terms of one or more calls to the
        stream's `read_some` and `write_some` operations.

        Upon a success, the input area of the stream buffer will
        hold the received message payload bytes (which may be zero
        in length). The functions @ref got_binary and @ref got_text
        may be used to query the stream and determine the type
        of the last received message.

        During reads, the implementation handles control frames as
        follows:

        @li A pong frame is sent when a ping frame is received.

        @li The @ref control_callback is invoked when a ping frame
            or pong frame is received.

        @li The WebSocket close procedure is started if a close frame
            is received. In this case, the operation will eventually
            complete with the error set to @ref error::closed.

        @param buffer A dynamic buffer to hold the message data after
        any masking or decompression has been applied.

        @throws system_error Thrown on failure.
    */
    template<class DynamicBuffer>
    void
    read(DynamicBuffer& buffer);

    /** Read a message from the stream.

        This function is used to synchronously read a message from
        the stream. The call blocks until one of the following is true:

        @li A complete message is received.

        @li An error occurs on the stream.

        This call is implemented in terms of one or more calls to the
        stream's `read_some` and `write_some` operations.

        Upon a success, the input area of the stream buffer will
        hold the received message payload bytes (which may be zero
        in length). The functions @ref got_binary and @ref got_text
        may be used to query the stream and determine the type
        of the last received message.

        During reads, the implementation handles control frames as
        follows:

        @li The @ref control_callback is invoked when a ping frame
            or pong frame is received.

        @li A pong frame is sent when a ping frame is received.

        @li The WebSocket close procedure is started if a close frame
            is received. In this case, the operation will eventually
            complete with the error set to @ref error::closed.

        @param buffer A dynamic buffer to hold the message data after
        any masking or decompression has been applied.

        @param ec Set to indicate what error occurred, if any.
    */
    template<class DynamicBuffer>
    void
    read(DynamicBuffer& buffer, error_code& ec);

    /** Start an asynchronous operation to read a message from the stream.

        This function is used to asynchronously read a message from
        the stream. The function call always returns immediately. The
        asynchronous operation will continue until one of the following
        is true:

        @li A complete message is received.

        @li An error occurs on the stream.

        This operation is implemented in terms of one or more calls to the
        next layer's `async_read_some` and `async_write_some` functions,
        and is known as a <em>composed operation</em>. The program must
        ensure that the stream performs no other reads until this operation
        completes.

        Upon a success, the input area of the stream buffer will
        hold the received message payload bytes (which may be zero
        in length). The functions @ref got_binary and @ref got_text
        may be used to query the stream and determine the type
        of the last received message.

        During reads, the implementation handles control frames as
        follows:

        @li The @ref control_callback is invoked when a ping frame
            or pong frame is received.

        @li A pong frame is sent when a ping frame is received.

        @li The WebSocket close procedure is started if a close frame
            is received. In this case, the operation will eventually
            complete with the error set to @ref error::closed.

        Because of the need to handle control frames, read operations
        can cause writes to take place. These writes are managed
        transparently; callers can still have one active asynchronous
        read and asynchronous write operation pending simultaneously
        (a user initiated call to @ref async_close counts as a write).

        @param buffer A dynamic buffer to hold the message data after
        any masking or decompression has been applied. This object must
        remain valid until the handler is called.

        @param handler The handler to be called when the read operation
        completes. Copies will be made of the handler as required. The
        function signature of the handler must be:
        @code
        void handler(
            error_code const& ec;   // Result of operation
        );
        @endcode
        Regardless of whether the asynchronous operation completes
        immediately or not, the handler will not be invoked from within
        this function. Invocation of the handler will be performed in a
        manner equivalent to using `boost::asio::io_service::post`.
    */
    template<class DynamicBuffer, class ReadHandler>
#if BOOST_BEAST_DOXYGEN
    void_or_deduced
#else
    async_return_type<
        ReadHandler, void(error_code)>
#endif
    async_read(DynamicBuffer& buffer, ReadHandler&& handler);

    //--------------------------------------------------------------------------

    /** Read some message data from the stream.

        This function is used to synchronously read some message
        data from the stream. The call blocks until one of the following
        is true:

        @li One or more message octets are placed into the provided buffers.

        @li A final message frame is received.

        @li A close frame is received and processed.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to the
        stream's `read_some` operation.

        During reads, the implementation handles control frames as
        follows:

        @li The @ref control_callback is invoked when any control
        frame is received.

        @li A pong frame is sent when a ping frame is received.

        @li The WebSocket close procedure is started if a close frame
            is received. In this case, the operation will eventually
            complete with the error set to @ref error::closed.

        @param buffer A dynamic buffer for holding the result

        @param limit An upper limit on the number of bytes this
        function will write. If this value is zero, then a reasonable
        size will be chosen automatically.

        @throws system_error Thrown on failure.

        @return The number of bytes written to the buffers
    */
    template<class DynamicBuffer>
    std::size_t
    read_some(
        DynamicBuffer& buffer,
        std::size_t limit);

    /** Read some message data from the stream.

        This function is used to synchronously read some message
        data from the stream. The call blocks until one of the following
        is true:

        @li One or more message octets are placed into the provided buffers.

        @li A final message frame is received.

        @li A close frame is received and processed.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to the
        stream's `read_some` operation.

        During reads, the implementation handles control frames as
        follows:

        @li The @ref control_callback is invoked when any control
        frame is received.

        @li A pong frame is sent when a ping frame is received.

        @li The WebSocket close procedure is started if a close frame
            is received. In this case, the operation will eventually
            complete with the error set to @ref error::closed.

        @param buffer A dynamic buffer for holding the result

        @param limit An upper limit on the number of bytes this
        function will write. If this value is zero, then a reasonable
        size will be chosen automatically.

        @param ec Set to indicate what error occurred, if any.

        @return The number of bytes written to the buffer
    */
    template<class DynamicBuffer>
    std::size_t
    read_some(
        DynamicBuffer& buffer,
        std::size_t limit,
        error_code& ec);

    /** Start an asynchronous operation to read some message data from the stream.

        This function is used to asynchronously read some message
        data from the stream. The function call always returns immediately.
        The asynchronous operation will continue until one of the following
        is true:

        @li One or more message octets are placed into the provided buffers.

        @li A final message frame is received.

        @li A close frame is received and processed.

        @li An error occurs on the stream.

        This operation is implemented in terms of one or more calls to the
        next layer's `async_read_some` function, and is known as a
        <em>composed operation</em>. The program must ensure that the
        stream performs no other reads until this operation completes.

        Upon a success, the input area of the stream buffer will
        hold the received message payload bytes (which may be zero
        in length). The functions @ref got_binary and @ref got_text
        may be used to query the stream and determine the type
        of the last received message.

        During reads, the implementation handles control frames as
        follows:

        @li The @ref control_callback is invoked when any control
        frame is received.

        @li A pong frame is sent when a ping frame is received.

        @li The WebSocket close procedure is started if a close frame
            is received. In this case, the operation will eventually
            complete with the error set to @ref error::closed.

        Because of the need to handle control frames, read operations
        can cause writes to take place. These writes are managed
        transparently; callers can still have one active asynchronous
        read and asynchronous write operation pending simultaneously
        (a user initiated call to @ref async_close counts as a write).

        @param buffer A dynamic buffer for holding the result

        @param handler The handler to be called when the read operation
        completes. Copies will be made of the handler as required. The
        function signature of the handler must be:
        @code
        void handler(
            error_code const& ec,           // Result of operation
            std::size_t bytes_transferred   // The number of bytes written to the buffer
        );
        @endcode
        Regardless of whether the asynchronous operation completes
        immediately or not, the handler will not be invoked from within
        this function. Invocation of the handler will be performed in a
        manner equivalent to using `boost::asio::io_service::post`.
    */
    template<class DynamicBuffer, class ReadHandler>
#if BOOST_BEAST_DOXYGEN
    void_or_deduced
#else
    async_return_type<
        ReadHandler, void(error_code, std::size_t)>
#endif
    async_read_some(
        DynamicBuffer& buffer,
        std::size_t limit,
        ReadHandler&& handler);

    //--------------------------------------------------------------------------

    /** Read some message data from the stream.

        This function is used to synchronously read some message
        data from the stream. The call blocks until one of the following
        is true:

        @li One or more message octets are placed into the provided buffers.

        @li A final message frame is received.

        @li A close frame is received and processed.

        @li An error occurs on the stream.

        This function is implemented in terms of one or more calls to the
        stream's `read_some` operation.

        During reads, the implementation handles control frames as
        follows:

        @li The @ref control_callback is invoked when any control
        frame is received.

        @li A pong frame is sent when a ping frame is received.

        @li The WebSocket close procedure is started if a close frame
            is received. In this case, the operation will eventually
            complete with the error set to @ref error::closed.

        @param buffers A mutable buffer to hold the message data after
        any masking or decompression has been applied.
        @param buffers The buffers into which message data will be
        placed after any masking or decompresison has been applied.
        The implementation will make copies of this object as needed,
        but ownership of the underlying memory is not transferred.
        The caller is responsible for ensuring that the memory
        locations pointed to by the buffers remains valid until the
        completion handler is called.

        @throws system_error Thrown on failure.

        @return The number of bytes written to the buffers
    */
    template<class MutableBufferSequence>
    std::size_t
    read_some(
        MutableBufferSequence const& buffers);

    /** Read some message data from the stream.

        This function is used to synchronously read some message
        data from the stream. The call blocks until one of the
        following is true:

        @li One or more message octets are placed into the provided buffers.

        @li A final message frame is received.

        @li A close frame is received and processed.

        @li An error occurs on the stream.

        This operation is implemented in terms of one or more calls to the
        stream's `read_some` function.

        During reads, the implementation handles control frames as
        follows:

        @li The @ref control_callback is invoked when any control
        frame is received.

        @li A pong frame is sent when a ping frame is received.

        @li The WebSocket close procedure is started if a close frame
            is received. In this case, the operation will eventually
            complete with the error set to @ref error::closed.

        @param buffers A mutable buffer to hold the message data after
        any masking or decompression has been applied.
        @param buffers The buffers into which message data will be
        placed after any masking or decompresison has been applied.
        The implementation will make copies of this object as needed,
        but ownership of the underlying memory is not transferred.
        The caller is responsible for ensuring that the memory
        locations pointed to by the buffers remains valid until the
        completion handler is called.

        @param ec Set to indicate what error occurred, if any.

        @return The number of bytes written to the buffers
    */
    template<class MutableBufferSequence>
    std::size_t
    read_some(
        MutableBufferSequence const& buffers,
        error_code& ec);

    /** Start an asynchronous operation to read some message data from the stream.

        This function is used to asynchronously read some message
        data from the stream. The function call always returns immediately.
        The asynchronous operation will continue until one of the following
        is true:

        @li One or more message octets are placed into the provided buffers.

        @li A final message frame is received.

        @li A close frame is received and processed.

        @li An error occurs on the stream.

        This operation is implemented in terms of one or more calls to the
        next layer's `async_read_some` function, and is known as a
        <em>composed operation</em>. The program must ensure that the
        stream performs no other reads until this operation completes.

        Upon a success, the input area of the stream buffer will
        hold the received message payload bytes (which may be zero
        in length). The functions @ref got_binary and @ref got_text
        may be used to query the stream and determine the type
        of the last received message.

        During reads, the implementation handles control frames as
        follows:

        @li The @ref control_callback is invoked when any control
        frame is received.

        @li A pong frame is sent when a ping frame is received.

        @li The WebSocket close procedure is started if a close frame
            is received. In this case, the operation will eventually
            complete with the error set to @ref error::closed.

        Because of the need to handle control frames, read operations
        can cause writes to take place. These writes are managed
        transparently; callers can still have one active asynchronous
        read and asynchronous write operation pending simultaneously
        (a user initiated call to @ref async_close counts as a write).

        @param buffers A mutable buffer to hold the message data after
        any masking or decompression has been applied.
        @param buffers The buffers into which message data will be
        placed after any masking or decompresison has been applied.
        The implementation will make copies of this object as needed,
        but ownership of the underlying memory is not transferred.
        The caller is responsible for ensuring that the memory
        locations pointed to by the buffers remains valid until the
        completion handler is called.

        @param handler The handler to be called when the read operation
        completes. Copies will be made of the handler as required. The
        function signature of the handler must be:
        @code
        void handler(
            error_code const& ec,           // Result of operation
            std::size_t bytes_transferred   // The number of bytes written to buffers
        );
        @endcode
        Regardless of whether the asynchronous operation completes
        immediately or not, the handler will not be invoked from within
        this function. Invocation of the handler will be performed in a
        manner equivalent to using `boost::asio::io_service::post`.
    */
    template<class MutableBufferSequence, class ReadHandler>
#if BOOST_BEAST_DOXYGEN
    void_or_deduced
#else
    async_return_type<ReadHandler, void(error_code, std::size_t)>
#endif
    async_read_some(
        MutableBufferSequence const& buffers,
        ReadHandler&& handler);

    //--------------------------------------------------------------------------
    //
    // Writing
    //
    //--------------------------------------------------------------------------

    /** Write a message to the stream.

        This function is used to synchronously write a message to
        the stream. The call blocks until one of the following conditions
        is met:

        @li The entire message is sent.

        @li An error occurs.

        This operation is implemented in terms of one or more calls to the
        next layer's `write_some` function.

        The current setting of the @ref binary option controls
        whether the message opcode is set to text or binary. If the
        @ref auto_fragment option is set, the message will be split
        into one or more frames as necessary. The actual payload contents
        sent may be transformed as per the WebSocket protocol settings.

        @param buffers The buffers containing the entire message
        payload. The implementation will make copies of this object
        as needed, but ownership of the underlying memory is not
        transferred. The caller is responsible for ensuring that
        the memory locations pointed to by buffers remains valid
        until the completion handler is called.

        @throws system_error Thrown on failure.

        @note This function always sends an entire message. To
        send a message in fragments, use @ref write_some.
    */
    template<class ConstBufferSequence>
    void
    write(ConstBufferSequence const& buffers);

    /** Write a message to the stream.

        This function is used to synchronously write a message to
        the stream. The call blocks until one of the following conditions
        is met:

        @li The entire message is sent.

        @li An error occurs.

        This operation is implemented in terms of one or more calls to the
        next layer's `write_some` function.

        The current setting of the @ref binary option controls
        whether the message opcode is set to text or binary. If the
        @ref auto_fragment option is set, the message will be split
        into one or more frames as necessary. The actual payload contents
        sent may be transformed as per the WebSocket protocol settings.

        @param buffers The buffers containing the entire message
        payload. The implementation will make copies of this object
        as needed, but ownership of the underlying memory is not
        transferred. The caller is responsible for ensuring that
        the memory locations pointed to by buffers remains valid
        until the completion handler is called.

        @param ec Set to indicate what error occurred, if any.

        @throws system_error Thrown on failure.

        @note This function always sends an entire message. To
        send a message in fragments, use @ref write_some.
    */
    template<class ConstBufferSequence>
    void
    write(ConstBufferSequence const& buffers, error_code& ec);

    /** Start an asynchronous operation to write a message to the stream.

        This function is used to asynchronously write a message to
        the stream. The function call always returns immediately.
        The asynchronous operation will continue until one of the
        following conditions is true:

        @li The entire message is sent.

        @li An error occurs.

        This operation is implemented in terms of one or more calls
        to the next layer's `async_write_some` functions, and is known
        as a <em>composed operation</em>. The program must ensure that
        the stream performs no other write operations (such as
        stream::async_write, stream::async_write_some, or
        stream::async_close).

        The current setting of the @ref binary option controls
        whether the message opcode is set to text or binary. If the
        @ref auto_fragment option is set, the message will be split
        into one or more frames as necessary. The actual payload contents
        sent may be transformed as per the WebSocket protocol settings.

        @param buffers The buffers containing the entire message
        payload. The implementation will make copies of this object
        as needed, but ownership of the underlying memory is not
        transferred. The caller is responsible for ensuring that
        the memory locations pointed to by buffers remains valid
        until the completion handler is called.

        @param handler The handler to be called when the write operation
        completes. Copies will be made of the handler as required. The
        function signature of the handler must be:
        @code
        void handler(
            error_code const& ec     // Result of operation
        );
        @endcode
        Regardless of whether the asynchronous operation completes
        immediately or not, the handler will not be invoked from within
        this function. Invocation of the handler will be performed in a
        manner equivalent to using `boost::asio::io_service::post`.
    */
    template<class ConstBufferSequence, class WriteHandler>
#if BOOST_BEAST_DOXYGEN
    void_or_deduced
#else
    async_return_type<
        WriteHandler, void(error_code)>
#endif
    async_write(ConstBufferSequence const& buffers,
        WriteHandler&& handler);

    /** Write partial message data on the stream.

        This function is used to write some or all of a message's
        payload to the stream. The call will block until one of the
        following conditions is true:

        @li A frame is sent.

        @li Message data is transferred to the write buffer.

        @li An error occurs.

        This operation is implemented in terms of one or more calls
        to the stream's `write_some` function.

        If this is the beginning of a new message, the message opcode
        will be set to text or binary as per the current setting of
        the @ref binary option. The actual payload sent may be
        transformed as per the WebSocket protocol settings.

        @param fin `true` if this is the last frame in the message.

        @param buffers The input buffer sequence holding the data to write.

        @return The number of bytes consumed in the input buffers.

        @throws system_error Thrown on failure.
    */
    template<class ConstBufferSequence>
    void
    write_some(bool fin, ConstBufferSequence const& buffers);

    /** Write partial message data on the stream.

        This function is used to write some or all of a message's
        payload to the stream. The call will block until one of the
        following conditions is true:

        @li A frame is sent.

        @li Message data is transferred to the write buffer.

        @li An error occurs.

        This operation is implemented in terms of one or more calls
        to the stream's `write_some` function.

        If this is the beginning of a new message, the message opcode
        will be set to text or binary as per the current setting of
        the @ref binary option. The actual payload sent may be
        transformed as per the WebSocket protocol settings.

        @param fin `true` if this is the last frame in the message.

        @param buffers The input buffer sequence holding the data to write.

        @param ec Set to indicate what error occurred, if any.

        @return The number of bytes consumed in the input buffers.
    */
    template<class ConstBufferSequence>
    void
    write_some(bool fin,
        ConstBufferSequence const& buffers, error_code& ec);

    /** Start an asynchronous operation to send a message frame on the stream.

        This function is used to asynchronously write a message frame
        on the stream. This function call always returns immediately.
        The asynchronous operation will continue until one of the following
        conditions is true:

        @li The entire frame is sent.

        @li An error occurs.

        This operation is implemented in terms of one or more calls
        to the next layer's `async_write_some` functions, and is known
        as a <em>composed operation</em>. The actual payload sent
        may be transformed as per the WebSocket protocol settings. The
        program must ensure that the stream performs no other write
        operations (such as stream::async_write, stream::async_write_some,
        or stream::async_close).

        If this is the beginning of a new message, the message opcode
        will be set to text or binary as per the current setting of
        the @ref binary option. The actual payload sent may be
        transformed as per the WebSocket protocol settings.

        @param fin A bool indicating whether or not the frame is the
        last frame in the corresponding WebSockets message.

        @param buffers A object meeting the requirements of
        ConstBufferSequence which holds the payload data before any
        masking or compression. Although the buffers object may be copied
        as necessary, ownership of the underlying buffers is retained by
        the caller, which must guarantee that they remain valid until
        the handler is called.

        @param handler The handler to be called when the write completes.
        Copies will be made of the handler as required. The equivalent
        function signature of the handler must be:
        @code void handler(
            error_code const& ec    // Result of operation
        ); @endcode
    */
    template<class ConstBufferSequence, class WriteHandler>
#if BOOST_BEAST_DOXYGEN
    void_or_deduced
#else
    async_return_type<
        WriteHandler, void(error_code)>
#endif
    async_write_some(bool fin,
        ConstBufferSequence const& buffers, WriteHandler&& handler);

private:
    enum class fail_how
    {
        code        = 1, // send close code, teardown, finish with error::failed
        close       = 2, // send frame in fb, teardown, finish with error::closed
        teardown    = 3  // teardown, finish with error::failed
    };

    template<class, class>  class accept_op;
    template<class>         class close_op;
    template<class>         class fail_op;
    template<class>         class handshake_op;
    template<class>         class ping_op;
    template<class>         class read_fh_op;
    template<class, class>  class read_some_op;
    template<class, class>  class read_op;
    template<class>         class response_op;
    template<class, class>  class write_some_op;
    template<class, class>  class write_op;

    static void default_decorate_req(request_type&) {}
    static void default_decorate_res(response_type&) {}

    void open(role_type role);
    void close();
    void reset();
    void wr_begin();

    template<class DynamicBuffer>
    bool
    parse_fh(detail::frame_header& fh,
        DynamicBuffer& b, close_code& code);

    template<class DynamicBuffer>
    std::size_t
    read_fh1(detail::frame_header& fh,
        DynamicBuffer& db, close_code& code);

    template<class DynamicBuffer>
    void
    read_fh2(detail::frame_header& fh,
        DynamicBuffer& db, close_code& code);

    template<class DynamicBuffer>
    void
    write_close(DynamicBuffer& db, close_reason const& rc);

    template<class DynamicBuffer>
    void
    write_ping(DynamicBuffer& db,
        detail::opcode op, ping_data const& data);

    template<class Decorator>
    request_type
    build_request(detail::sec_ws_key_type& key,
        string_view host,
            string_view target,
                Decorator const& decorator);

    template<class Body,
        class Allocator, class Decorator>
    response_type
    build_response(http::request<Body,
        http::basic_fields<Allocator>> const& req,
            Decorator const& decorator);

    template<class Decorator>
    void
    do_accept(Decorator const& decorator,
        error_code& ec);

    template<class Body, class Allocator,
        class Decorator>
    void
    do_accept(http::request<Body,
        http::basic_fields<Allocator>> const& req,
            Decorator const& decorator, error_code& ec);

    template<class RequestDecorator>
    void
    do_handshake(response_type* res_p,
        string_view host, string_view target,
            RequestDecorator const& decorator,
                error_code& ec);

    void
    do_response(response_type const& resp,
        detail::sec_ws_key_type const& key, error_code& ec);
};

} // websocket
} // beast
} // boost

#include <boost/beast/websocket/impl/accept.ipp>
#include <boost/beast/websocket/impl/close.ipp>
#include <boost/beast/websocket/impl/fail.ipp>
#include <boost/beast/websocket/impl/handshake.ipp>
#include <boost/beast/websocket/impl/ping.ipp>
#include <boost/beast/websocket/impl/read.ipp>
#include <boost/beast/websocket/impl/stream.ipp>
#include <boost/beast/websocket/impl/write.ipp>

#endif
