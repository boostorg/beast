//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_WRITE_HPP
#define BEAST_HTTP_WRITE_HPP

#include <beast/config.hpp>
#include <beast/core/buffer_cat.hpp>
#include <beast/core/consuming_buffers.hpp>
#include <beast/core/multi_buffer.hpp>
#include <beast/http/message.hpp>
#include <beast/http/detail/chunk_encode.hpp>
#include <beast/core/error.hpp>
#include <beast/core/async_result.hpp>
#include <beast/core/string_view.hpp>
#include <boost/variant.hpp>
#include <limits>
#include <memory>
#include <ostream>
#include <type_traits>
#include <utility>

namespace beast {
namespace http {

/** A chunk decorator which does nothing.

    When selected as a chunk decorator, objects of this type
    affect the output of messages specifying chunked
    transfer encodings as follows:

    @li chunk headers will have empty chunk extensions, and

    @li final chunks will have an empty set of trailers.

    @see @ref serializer
*/
struct empty_decorator
{
    template<class ConstBufferSequence>
    string_view
    operator()(ConstBufferSequence const&) const
    {
        return {"\r\n"};
    }

    string_view
    operator()(boost::asio::null_buffers) const
    {
        return {};
    }
};

/** Provides stream-oriented HTTP message serialization functionality.

    Objects of this type may be used to perform synchronous or
    asynchronous serialization of an HTTP message on a stream.
    Unlike functions such as @ref write or @ref async_write,
    the stream operations provided here guarantee that bounded
    work will be performed. This is accomplished by making one
    or more calls to the underlying stream's `write_some` or
    `async_write_some` member functions. In order to fully
    serialize the message, multiple calls are required.

    The ability to incrementally serialize a message, peforming
    bounded work at each iteration is useful in many scenarios,
    such as:

    @li Setting consistent, per-call timeouts

    @li Efficiently relaying body content from another stream

    @li Performing application-layer flow control

    To use this class, construct an instance with the message
    to be sent. To make it easier to declare the type, the
    helper function @ref make_serializer is provided:

    The implementation will automatically perform chunk encoding
    if the contents of the message indicate that chunk encoding
    is required. If the semantics of the message indicate that
    the connection should be closed after the message is sent,
    the error delivered from stream operations will be
    `boost::asio::error::eof`.

    @code
    template<class Stream>
    void send(Stream& stream, request<string_body> const& msg)
    {
        serializer<true, string_body, fields> w{msg};
        do
        {
            w.write_some();
        }
        while(! w.is_done());
    }
    @endcode

    Upon construction, an optional chunk decorator may be
    specified. This decorator is a function object called with
    each buffer sequence of the body when the chunked transfer
    encoding is indicate in the message header. The decorator
    will be called with an empty buffer sequence (actually
    the type `boost::asio::null_buffers`) to indicate the
    final chunk. The decorator may return a string which forms
    the chunk extension for chunks, and the field trailers
    for the final chunk.

    In C++11 the decorator must be declared as a class or
    struct with a templated operator() thusly:

    @code
        // The implementation guarantees that operator()
        // will be called only after the view returned by
        // any previous calls to operator() are no longer
        // needed. The decorator instance is intended to
        // manage the lifetime of the storage for all returned
        // views.
        //
        struct decorator
        {
            // Returns the chunk-extension for each chunk.
            // The buffer returned must include a trailing "\r\n",
            // and the leading semicolon (";") if one or more
            // chunk extensions are specified.
            //
            template<class ConstBufferSequence>
            string_view
            operator()(ConstBufferSequence const&) const;

            // Returns a set of field trailers for the final chunk.
            // Each field should be formatted according to rfc7230
            // including the trailing "\r\n" for each field. If
            // no trailers are indicated, an empty string is returned.
            //
            string_view
            operator()(boost::asio::null_buffers) const;
        };
    @endcode

    @par Thread Safety
    @e Distinct @e objects: Safe.@n
    @e Shared @e objects: Unsafe.

    @tparam isRequest `true` if the message is a request.

    @tparam Body The body type of the message.

    @tparam Fields The type of fields in the message.

    @tparam Decorator The type of chunk decorator to use.

    @tparam Allocator The type of allocator to use.

    @see @ref make_serializer
*/
template<
    bool isRequest, class Body, class Fields,
    class Decorator = empty_decorator,
    class Allocator = std::allocator<char>
>
class serializer
{
    template<class Stream, class Handler>
    class async_op;

    enum
    {
        do_init             =   0,
        do_header_only      =  10,
        do_header           =  20,
        do_body             =  40,
        
        do_init_c           =  50,
        do_header_only_c    =  60,
        do_header_c         =  70,
        do_body_c           =  90,
        do_final_c          = 100,

        do_complete         = 110
    };

    void split(bool, std::true_type) {}
    void split(bool v, std::false_type) { split_ = v; }

    using buffer_type =
        basic_multi_buffer<Allocator>;

    using reader = typename Body::reader;

    using is_deferred =
        typename reader::is_deferred;

    using cb0_t = consuming_buffers<buffers_view<
        typename buffer_type::const_buffers_type,   // header
        typename reader::const_buffers_type>>; // body

    using cb1_t = consuming_buffers<
        typename reader::const_buffers_type>;  // body

    using ch0_t = consuming_buffers<buffers_view<
        typename buffer_type::const_buffers_type,   // header
        detail::chunk_header,                       // chunk-header
        boost::asio::const_buffers_1,               // chunk-ext+\r\n
        typename reader::const_buffers_type,   // body
        boost::asio::const_buffers_1>>;             // crlf
    
    using ch1_t = consuming_buffers<buffers_view<
        detail::chunk_header,                       // chunk-header
        boost::asio::const_buffers_1,               // chunk-ext+\r\n
        typename reader::const_buffers_type,   // body
        boost::asio::const_buffers_1>>;             // crlf

    using ch2_t = consuming_buffers<buffers_view<
        boost::asio::const_buffers_1,               // chunk-final
        boost::asio::const_buffers_1,               // trailers 
        boost::asio::const_buffers_1>>;             // crlf

    message<isRequest, Body, Fields> const& m_;
    Decorator d_;
    std::size_t limit_ =
        (std::numeric_limits<std::size_t>::max)();
    boost::optional<reader> rd_;
    buffer_type b_;
    boost::variant<boost::blank,
        cb0_t, cb1_t, ch0_t, ch1_t, ch2_t> v_;
    int s_;
    bool split_ = is_deferred::value;
    bool header_done_ = false;
    bool chunked_;
    bool close_;
    bool more_;

public:
    /** Constructor

        @param msg The message to serialize. The message object
        must remain valid for the lifetime of the write stream.

        @param decorator An optional decorator to use.

        @param alloc An optional allocator to use.
    */
    explicit
    serializer(message<isRequest, Body, Fields> const& msg,
        Decorator const& decorator = Decorator{},
            Allocator const& alloc = Allocator{});

    /// Returns the maximum number of bytes that will be written in each operation
    std::size_t
    limit() const
    {
        return limit_;
    }

    /** Returns `true` if we will pause after writing the header.
    */
    bool
    split() const
    {
        return split_;
    }

    /** Set whether the header and body are written separately.

        When the split feature is enabled, the implementation will
        write only the octets corresponding to the serialized header
        first. If the header has already been written, this function
        will have no effect on output. This function should be called
        before any writes take place, otherwise the behavior is
        undefined.
    */
    void
    split(bool v)
    {
        split(v, is_deferred{});
    }

    /** Set the maximum number of bytes that will be written in each operation.

        By default, there is no limit on the size of writes.

        @param n The byte limit. This must be greater than zero.
    */
    void
    limit(std::size_t n)
    {
        limit_ = n;
    }

    /** Return `true` if serialization of the header is complete.

        This function indicates whether or not all octets
        corresponding to the serialized representation of the
        header have been successfully delivered to the stream.
    */
    bool
    is_header_done() const
    {
        return header_done_;
    }

    /** Return `true` if serialization is complete

        The operation is complete when all octets corresponding
        to the serialized representation of the message have been
        successfully delivered to the stream.
    */
    bool
    is_done() const
    {
        return s_ == do_complete;
    }

    /** Write some serialized message data to the stream.

        This function is used to write serialized message data to the
        stream. The function call will block until one of the following
        conditions is true:
        
        @li One or more bytes have been transferred.

        @li An error occurs on the stream.

        In order to completely serialize a message, this function
        should be called until @ref is_done returns `true`. If the
        semantics of the message indicate that the connection should
        be closed after the message is sent, the error delivered from
        this call will be `boost::asio::error::eof`.
    
        @param stream The stream to write to. This type must
        satisfy the requirements of @b SyncWriteStream.

        @throws system_error Thrown on failure.
    */
    template<class SyncWriteStream>
    void
    write_some(SyncWriteStream& stream);

    /** Write some serialized message data to the stream.

        This function is used to write serialized message data to the
        stream. The function call will block until one of the following
        conditions is true:
        
        @li One or more bytes have been transferred.

        @li An error occurs on the stream.

        In order to completely serialize a message, this function
        should be called until @ref is_done returns `true`. If the
        semantics of the message indicate that the connection should
        be closed after the message is sent, the error delivered from
        this call will be `boost::asio::error::eof`.

        @param stream The stream to write to. This type must
        satisfy the requirements of @b SyncWriteStream.

        @param ec Set to indicate what error occurred, if any.
    */
    template<class SyncWriteStream>
    void
    write_some(SyncWriteStream& stream, error_code &ec);

    /** Start an asynchronous write of some serialized message data.

        This function is used to asynchronously write serialized
        message data to the stream. The function call always returns
        immediately. The asynchronous operation will continue until
        one of the following conditions is true:

        @li One or more bytes have been transferred.

        @li An error occurs on the stream.

        In order to completely serialize a message, this function
        should be called until @ref is_done returns `true`. If the
        semantics of the message indicate that the connection should
        be closed after the message is sent, the error delivered from
        this call will be `boost::asio::error::eof`.

        @param stream The stream to write to. This type must
        satisfy the requirements of @b SyncWriteStream.

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
    template<class AsyncWriteStream, class WriteHandler>
#if BEAST_DOXYGEN
    void_or_deduced
#else
    async_return_type<WriteHandler, void(error_code)>
#endif
    async_write_some(AsyncWriteStream& stream,
        WriteHandler&& handler);
};

/** Return a stateful object to serialize an HTTP message.

    This convenience function makes it easier to declare
    the variable for a given message.
*/
template<
    bool isRequest, class Body, class Fields,
    class Decorator = empty_decorator,
    class Allocator = std::allocator<char>>
inline
serializer<isRequest, Body, Fields,
    typename std::decay<Decorator>::type,
    typename std::decay<Allocator>::type>
make_serializer(message<isRequest, Body, Fields> const& m,
    Decorator const& decorator = Decorator{},
        Allocator const& allocator = Allocator{})
{
    return serializer<isRequest, Body, Fields,
        typename std::decay<Decorator>::type,
        typename std::decay<Allocator>::type>{
            m, decorator, allocator};
}

//------------------------------------------------------------------------------

/** Write a HTTP/1 message to a stream.

    This function is used to write a message to a stream. The call
    will block until one of the following conditions is true:

    @li The entire message is written.

    @li An error occurs.

    This operation is implemented in terms of one or more calls
    to the stream's `write_some` function.

    The implementation will automatically perform chunk encoding if
    the contents of the message indicate that chunk encoding is required.
    If the semantics of the message indicate that the connection should
    be closed after the message is sent, the error thrown from this
    function will be `boost::asio::error::eof`.

    @param stream The stream to which the data is to be written.
    The type must support the @b SyncWriteStream concept.

    @param msg The message to write.

    @throws system_error Thrown on failure.
*/
template<class SyncWriteStream,
    bool isRequest, class Body, class Fields>
void
write(SyncWriteStream& stream,
    message<isRequest, Body, Fields> const& msg);

/** Write a HTTP/1 message on a stream.

    This function is used to write a message to a stream. The call
    will block until one of the following conditions is true:

    @li The entire message is written.

    @li An error occurs.

    This operation is implemented in terms of one or more calls
    to the stream's `write_some` function.

    The implementation will automatically perform chunk encoding if
    the contents of the message indicate that chunk encoding is required.
    If the semantics of the message indicate that the connection should
    be closed after the message is sent, the error returned from this
    function will be `boost::asio::error::eof`.

    @param stream The stream to which the data is to be written.
    The type must support the @b SyncWriteStream concept.

    @param msg The message to write.

    @param ec Set to the error, if any occurred.
*/
template<class SyncWriteStream,
    bool isRequest, class Body, class Fields>
void
write(SyncWriteStream& stream,
    message<isRequest, Body, Fields> const& msg,
        error_code& ec);

/** Write a HTTP/1 message asynchronously to a stream.

    This function is used to asynchronously write a message to
    a stream. The function call always returns immediately. The
    asynchronous operation will continue until one of the following
    conditions is true:

    @li The entire message is written.

    @li An error occurs.

    This operation is implemented in terms of one or more calls to
    the stream's `async_write_some` functions, and is known as a
    <em>composed operation</em>. The program must ensure that the
    stream performs no other write operations until this operation
    completes.

    The implementation will automatically perform chunk encoding if
    the contents of the message indicate that chunk encoding is required.
    If the semantics of the message indicate that the connection should
    be closed after the message is sent, the operation will complete with
    the error set to `boost::asio::error::eof`.

    @param stream The stream to which the data is to be written.
    The type must support the @b AsyncWriteStream concept.

    @param msg The message to write. The object must remain valid
    at least until the completion handler is called; ownership is
    not transferred.

    @param handler The handler to be called when the operation
    completes. Copies will be made of the handler as required.
    The equivalent function signature of the handler must be:
    @code void handler(
        error_code const& error // result of operation
    ); @endcode
    Regardless of whether the asynchronous operation completes
    immediately or not, the handler will not be invoked from within
    this function. Invocation of the handler will be performed in a
    manner equivalent to using `boost::asio::io_service::post`.
*/
template<class AsyncWriteStream,
    bool isRequest, class Body, class Fields,
        class WriteHandler>
async_return_type<
    WriteHandler, void(error_code)>
async_write(AsyncWriteStream& stream,
    message<isRequest, Body, Fields> const& msg,
        WriteHandler&& handler);

//------------------------------------------------------------------------------

/** Serialize a HTTP/1 header to a `std::ostream`.

    The function converts the header to its HTTP/1 serialized
    representation and stores the result in the output stream.

    @param os The output stream to write to.

    @param msg The message fields to write.
*/
template<bool isRequest, class Fields>
std::ostream&
operator<<(std::ostream& os,
    header<isRequest, Fields> const& msg);

/** Serialize a HTTP/1 message to a `std::ostream`.

    The function converts the message to its HTTP/1 serialized
    representation and stores the result in the output stream.

    The implementation will automatically perform chunk encoding if
    the contents of the message indicate that chunk encoding is required.

    @param os The output stream to write to.

    @param msg The message to write.
*/
template<bool isRequest, class Body, class Fields>
std::ostream&
operator<<(std::ostream& os,
    message<isRequest, Body, Fields> const& msg);

} // http
} // beast

#include <beast/http/impl/write.ipp>

#endif
