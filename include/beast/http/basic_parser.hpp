//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_BASIC_PARSER_HPP
#define BEAST_HTTP_BASIC_PARSER_HPP

#include <beast/config.hpp>
#include <beast/core/error.hpp>
#include <beast/core/string_view.hpp>
#include <beast/http/detail/basic_parser.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/optional.hpp>
#include <boost/assert.hpp>
#include <memory>
#include <utility>

namespace beast {
namespace http {

/** Describes the parser's current state.

    The state is expressed as the type of data that
    @ref basic_parser is expecting to see in subsequently
    provided octets.
*/
enum class parse_state
{
    /// Expecting one or more header octets
    header = 0,

    /// Expecting one or more body octets
    body = 1,

    /// Expecting zero or more body octets followed by EOF
    body_to_eof = 2,

    /// Expecting additional chunk header octets
    chunk_header = 3,

    /// Expecting one or more chunk body octets
    chunk_body = 4,

    /** The parsing is complete.

        The parse is considered complete when the full header
        is received and either the full body is received, or
        the semantics of the message indicate that no body
        is expected. This includes the case where the caller
        has indicated to the parser that no body is expected,
        for example when receiving a response to a HEAD request.
    */
    complete = 5
};

/** A parser for decoding HTTP/1 wire format messages.

    This parser is designed to efficiently parse messages in the
    HTTP/1 wire format. It allocates no memory when input is
    presented as a single contiguous buffer, and uses minimal
    state. It will handle chunked encoding and it understands
    the semantics of the Connection, Content-Length, and Upgrade
    fields.

    The interface uses CRTP (Curiously Recurring Template Pattern).
    To use this class, derive from @ref basic_parser. When bytes
    are presented, the implementation will make a series of zero
    or more calls to derived class members functions (referred to
    as "callbacks" from here on) matching a specific signature.

    Every callback must be provided by the derived class, or else
    a compilation error will be generated. This exemplar shows
    the signature and description of the callbacks required in
    the derived class.

    @par Derived Example

    @code
    template<bool isRequest>
    struct derived
        : basic_parser<isRequest, derived<isRequest>>
    {
        // The type used when providing a mutable
        // buffer sequence in which to store body data.
        //
        using mutable_buffers_type = ...;

        // When isRequest == true, called
        // after the Request Line is received.
        //
        void
        on_request(
            string_view const& method,
            string_view const& target,
            int version,
            error_code& ec);

        // When isRequest == false, called
        // after the Status Line is received.
        //
        void
        on_response(
            int status,
            string_view const& reason,
            int version,
            error_code& ec);

        // Called after receiving a field/value pair.
        //
        void
        on_field(
            string_view const& name,
            string_view const& value,
            error_code& ec);

        // Called after the header is complete.
        //
        void
        on_header(
            error_code& ec);

        // Called once before the body, if any, is started.
        // This will only be called if the semantics of the
        // message indicate that a body exists, including
        // an indicated body of zero length.
        //
        void
        on_body();

        // Called zero or more times to provide body data.
        //
        // Only used if isDirect == false
        //
        void
        on_data(
            string_view const& s,
            error_code& ec);

        // Called zero or more times to retrieve a mutable
        // buffer sequence in which to store body data.
        //
        // Only used if isDirect == true
        //
        mutable_buffers_type
        on_prepare(
            std::size_t n);

        // Called after body data has been stored in the
        // buffer returned by the previous call to on_prepare.
        //
        // Only used if isDirect == true
        //
        void
        on_commit(
            std::size_t n);

        // If the Transfer-Encoding is specified, and the
        // last item in the list of encodings is "chunked",
        // called after receiving a chunk header or a final
        // chunk.
        //
        void
        on_chunk(
            std::uint64_t length,           // Length of this chunk
            string_view const& ext,   // The chunk extensions, if any
            error_code& ec);

        // Called once when the message is complete.
        // This will be called even if there is no body.
        //
        void
        on_complete(error_code& ec);
    };
    @endcode

    If a callback sets the error code, the error will be propagated
    to the caller of the parser. Behavior of parsing after an error
    is returned is undefined.

    When the parser state is positioned to read bytes belonging to
    the body, calling @ref write or @ref write will implicitly
    cause a buffer copy (because bytes are first transferred to the
    dynamic buffer). To avoid this copy, the additional functions
    @ref copy_body, @ref prepare_body, and @ref commit_body are
    provided to allow the caller to read bytes directly into buffers
    supplied by the parser.

    The parser is optimized for the case where the input buffer
    sequence consists of a single contiguous buffer. The
    @ref beast::flat_buffer class is provided, which guarantees
    that the input sequence of the stream buffer will be represented
    by exactly one contiguous buffer. To ensure the optimum performance
    of the parser, use @ref beast::flat_buffer with HTTP algorithms
    such as @ref beast::http::read, @ref beast::http::read_some,
    @ref beast::http::async_read, and @ref beast::http::async_read_some.
    Alternatively, the caller may use custom techniques to ensure that
    the structured portion of the HTTP message (header or chunk header)
    is contained in a linear buffer.

    @tparam isRequest A `bool` indicating whether the parser will be
    presented with request or response message.

    @tparam isDirect A `bool` indicating whether the parser interface
    supports reading body data directly into parser-provided buffers.

    @tparam Derived The derived class type. This is part of the
    Curiously Recurring Template Pattern interface.
*/
template<bool isRequest, bool isDirect, class Derived>
class basic_parser
    : private detail::basic_parser_base
{
    template<bool OtherIsRequest,
        bool OtherIsDirect, class OtherDerived>
    friend class basic_parser;

    // Message will be complete after reading header
    static unsigned constexpr flagSkipBody              = 1<<  0;



    static unsigned constexpr flagOnBody                = 1<<  1;

    // The parser has read at least one byte
    static unsigned constexpr flagGotSome               = 1<<  2;

    // Message semantics indicate a body is expected.
    // cleared if flagSkipBody set
    //
    static unsigned constexpr flagHasBody               = 1<<  3;

    static unsigned constexpr flagHTTP11                = 1<<  4;
    static unsigned constexpr flagNeedEOF               = 1<<  5;
    static unsigned constexpr flagExpectCRLF            = 1<<  6;
    static unsigned constexpr flagFinalChunk            = 1<<  7;
    static unsigned constexpr flagConnectionClose       = 1<<  8;
    static unsigned constexpr flagConnectionUpgrade     = 1<<  9;
    static unsigned constexpr flagConnectionKeepAlive   = 1<< 10;
    static unsigned constexpr flagContentLength         = 1<< 11;
    static unsigned constexpr flagChunked               = 1<< 12;
    static unsigned constexpr flagUpgrade               = 1<< 13;

    std::uint64_t len_;     // size of chunk or body
    std::unique_ptr<char[]> buf_;
    std::size_t buf_len_ = 0;
    std::size_t skip_ = 0;  // search from here
    std::size_t x_;         // scratch variable
    unsigned f_ = 0;        // flags
    parse_state state_ = parse_state::header;
    string_view ext_;
    string_view body_;

public:
    /// Copy constructor (disallowed)
    basic_parser(basic_parser const&) = delete;

    /// Copy assignment (disallowed)
    basic_parser& operator=(basic_parser const&) = delete;

    /// Default constructor
    basic_parser() = default;

    /// `true` if this parser parses requests, `false` for responses.
    static bool constexpr is_request = isRequest;

    /// Destructor
    ~basic_parser() = default;

    /** Move constructor

        After the move, the only valid operation on the
        moved-from object is destruction.
    */
    template<bool OtherIsDirect, class OtherDerived>
    basic_parser(basic_parser<
        isRequest, OtherIsDirect, OtherDerived>&&);

    /** Set the skip body option.

        The option controls whether or not the parser expects to
        see an HTTP body, regardless of the presence or absence of
        certain fields such as Content-Length.

        Depending on the request, some responses do not carry a body.
        For example, a 200 response to a CONNECT request from a
        tunneling proxy. In these cases, callers may use this function
        inform the parser that no body is expected. The parser will
        consider the message complete after the header has been received.

        @note This function must called before any bytes are processed.
    */
    void
    skip_body();

    /** Returns the current parser state.

        The parser state indicates what octets the parser
        expects to see next in the input stream.
    */
    parse_state
    state() const
    {
        return state_;
    }

    /// Returns `true` if the parser has received at least one byte of input.
    bool
    got_some() const
    {
        return (f_ & flagGotSome) != 0;
    }

    /// Returns `true` if the complete header has been parsed.
    bool
    got_header() const
    {
        return state_ != parse_state::header;
    }

    /** Returns `true` if a Content-Length is specified.

        @note Only valid after parsing a complete header.
    */
    bool
    got_content_length() const
    {
        return (f_ & flagContentLength) != 0;
    }

    /** Returns `true` if the message is complete.

        The message is complete after a full header is
        parsed and one of the following is true:

        @li @ref skip_body was called

        @li The semantics of the message indicate there is no body.

        @li The semantics of the message indicate a body is
        expected, and the entire body was received.
    */
    bool
    is_complete() const
    {
        return state_ == parse_state::complete;
    }

    /** Returns `true` if the message is an upgrade message.

        @note Only valid after parsing a complete header.
    */
    bool
    is_upgrade() const
    {
        return (f_ & flagConnectionUpgrade) != 0;
    }

    /** Returns `true` if keep-alive is specified

        @note Only valid after parsing a complete header.
    */
    bool
    is_keep_alive() const;

    /** Returns `true` if the chunked Transfer-Encoding is specified.

        @note Only valid after parsing a complete header.
    */
    bool
    is_chunked() const
    {
        return (f_ & flagChunked) != 0;
    }

    /** Write part of a buffer sequence to the parser.

        This function attempts to parse the HTTP message
        stored in the caller provided buffers. Upon success,
        a positive return value indicates that the parser
        made forward progress, consuming that number of
        bytes.

        A return value of zero indicates that the parser
        requires additional input. In this case the caller
        should append additional bytes to the input buffer
        sequence and call @ref write again.

        @param buffers An object meeting the requirements of
        @b ConstBufferSequence that represents the message.

        @param ec Set to the error, if any occurred.

        @return The number of bytes consumed in the buffer
        sequence.
    */
    template<class ConstBufferSequence>
    std::size_t
    write(ConstBufferSequence const& buffers, error_code& ec);

#if ! BEAST_DOXYGEN
    std::size_t
    write(boost::asio::const_buffers_1 const& buffer,
        error_code& ec);
#endif

    /** Inform the parser that the end of stream was reached.

        In certain cases, HTTP needs to know where the end of
        the stream is. For example, sometimes servers send
        responses without Content-Length and expect the client
        to consume input (for the body) until EOF. Callbacks
        and errors will still be processed as usual.

        This is typically called when a read from the
        underlying stream object sets the error code to
        `boost::asio::error::eof`.

        @note Only valid after parsing a complete header.

        @param ec Set to the error, if any occurred. 
    */
    void
    write_eof(error_code& ec);

    /** Returns the number of bytes remaining in the body or chunk.

        If a Content-Length is specified and the parser state
        is equal to @ref beast::http::parse_state::body, this will return
        the number of bytes remaining in the body. If the
        chunked Transfer-Encoding is indicated and the parser
        state is equal to @ref beast::http::parse_state::chunk_body, this
        will return the number of bytes remaining in the chunk.
        Otherwise, the function behavior is undefined.
    */
    std::uint64_t
    size() const
    {
        BOOST_ASSERT(
            state_ == parse_state::body ||
            state_ == parse_state::chunk_body);
        return len_;
    }

    /** Returns the body data parsed in the last call to @ref write.

        This buffer is invalidated after any call to @ref write
        or @ref write_eof.

        @note If the last call to @ref write came from the input
        area of a @b DynamicBuffer object, a call to the dynamic
        buffer's `consume` function may invalidate this return
        value.
    */
    string_view const&
    body() const
    {
        // This function not available when isDirect==true
        static_assert(! isDirect, "");
        return body_;
    }

    /** Returns the chunk extension parsed in the last call to @ref write.

        This buffer is invalidated after any call to @ref write
        or @ref write_eof.

        @note If the last call to @ref write came from the input
        area of a @b DynamicBuffer object, a call to the dynamic
        buffer's `consume` function may invalidate this return
        value.
    */
    string_view const&
    chunk_extension() const
    {
        // This function not available when isDirect==true
        static_assert(! isDirect, "");
        return ext_;
    }

    /** Returns the optional value of Content-Length if known.

        @note The return value is undefined unless a complete
        header has been parsed.
    */
    boost::optional<std::uint64_t>
    content_length() const
    {
        BOOST_ASSERT(got_header());
        if(! (f_ & flagContentLength))
            return boost::none;
        return len_;
    }

    /** Copy leftover body data from the dynamic buffer.

        @note This member function is only available when
        `isDirect==true`.

        @return The number of bytes processed from the dynamic
        buffer. The caller should remove these bytes by calling
        `consume` on the buffer.
    */
    template<class DynamicBuffer>
    std::size_t
    copy_body(DynamicBuffer& buffer);

    /** Returns a set of buffers for storing body data.

        @param buffers A writable output parameter into which
        the function will place the buffers upon success.

        @param limit The maximum number of bytes in the
        size of the returned buffer sequence. The actual size
        of the buffer sequence may be lower than this number.

        @note This member function is only available when
        `isDirect==true`.
    */
    template<class MutableBufferSequence>
    void
    prepare_body(boost::optional<
        MutableBufferSequence>& buffers, std::size_t limit);

    /** Commit body data.

        @param n The number of bytes to commit. This must
        be less than or equal to the size of the buffer
        sequence returned by @ref prepare_body.

        @note This member function is only available when
        `isDirect==true`.
    */
    void
    commit_body(std::size_t n);

    /** Indicate that body octets have been consumed.

        @param n The number of bytes to consume.
    */
    void
    consume(std::size_t n)
    {
        BOOST_ASSERT(n <= len_);
        BOOST_ASSERT(
            state_ == parse_state::body ||
            state_ == parse_state::chunk_body);
        len_ -= n;
        if(len_ == 0)
        {
            if(state_ == parse_state::body)
                state_ = parse_state::complete;
            else
                state_ = parse_state::chunk_header;
        }
    }

private:
    inline
    Derived&
    impl()
    {
        return *static_cast<Derived*>(this);
    }

    template<class ConstBufferSequence>
    string_view
    maybe_flatten(
        ConstBufferSequence const& buffers);

    std::size_t
    do_write(boost::asio::const_buffers_1 const& buffer,
        error_code& ec, std::true_type);

    std::size_t
    do_write(boost::asio::const_buffers_1 const& buffer,
        error_code& ec, std::false_type);

    void
    parse_startline(char const*& it,
        int& version, int& status,
            error_code& ec, std::true_type);

    void
    parse_startline(char const*& it,
        int& version, int& status,
            error_code& ec, std::false_type);

    void
    parse_fields(char const*& it,
        char const* last, error_code& ec);

    void
    do_field(
        string_view const& name,
            string_view const& value,
                error_code& ec);

    std::size_t
    parse_header(char const* p,
        std::size_t n, error_code& ec);

    void
    do_header(int, std::true_type);

    void
    do_header(int status, std::false_type);

    void
    maybe_do_body_direct();

    void
    maybe_do_body_indirect(error_code& ec);

    std::size_t
    parse_chunk_header(char const* p,
        std::size_t n, error_code& ec);

    std::size_t
    parse_body(char const* p,
        std::size_t n, error_code& ec);

    std::size_t
    parse_body_to_eof(char const* p,
        std::size_t n, error_code& ec);

    std::size_t
    parse_chunk_body(char const* p,
        std::size_t n, error_code& ec);

    void
    do_complete(error_code& ec);
};

} // http
} // beast

#include <beast/http/impl/basic_parser.ipp>

#endif
