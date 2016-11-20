//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_BASIC_PARSER_HPP
#define BEAST_HTTP_BASIC_PARSER_HPP

#include <beast/core/error.hpp>
#include <beast/http/detail/basic_parser.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/optional.hpp>
#include <boost/assert.hpp>
#include <boost/utility/string_ref.hpp>
#include <utility>

namespace beast {
namespace http {

/** Body maximum size option.

    Sets the maximum number of cumulative bytes allowed including
    all body octets. Octets in chunk-encoded bodies are counted
    after decoding. A value of zero indicates no limit on
    the number of body octets.

    The default body maximum size for requests is 4MB (four
    megabytes or 4,194,304 bytes) and unlimited for responses.

    @note Objects of this type are used with @ref basic_parser::set_option.
*/
struct body_max_size
{
    std::size_t value;

    explicit
    body_max_size(std::size_t v)
        : value(v)
    {
    }
};

/** Header maximum size option.

    Sets the maximum number of cumulative bytes allowed
    including all header octets. A value of zero indicates
    no limit on the number of header octets.

    The default header maximum size is 16KB (16,384 bytes).

    @note Objects of this type are used with @ref basic_parser::set_option.
*/
struct header_max_size
{
    std::size_t value;

    explicit
    header_max_size(std::size_t v)
        : value(v)
    {
    }
};

/** Skip body option.

    The option controls whether or not the parser expects to see a
    HTTP body, regardless of the presence or absence of certain fields
    such as Content-Length.

    Depending on the request, some responses do not carry a body.
    For example, a 200 response to a CONNECT request from a tunneling
    proxy. In these cases, callers use the @ref skip_body option to
    inform the parser that no body is expected. The parser will consider
    the message complete after the header has been received.

    Example:
    @code
        parser<true> p;
        p.set_option(skip_body{true});
    @endcode
    @note Objects of this type are passed to @ref basic_parser::set_option.
*/
struct skip_body
{
    bool value;

    explicit
    skip_body(bool v)
        : value(v)
    {
    }
};

/** Information about the body or body chunk being parsed.
*/
struct body_info
{
    /** The type of body being parsed.

        The values are as follows:

        @li 0 The end of body is defined by the end of file.

        @li 1 A Content-Length was specified.

        @li 2 The body uses the chunked Transfer-Encoding.
    */
    int type;

    /** The size of the body chunk.

        If a Content-Length is specified, this will be the
        number of bytes remaining in the message body.
    */
    std::uint64_t length;
};

/** A parser for decoding HTTP/1 wire format messages.

    This parser is designed to efficiently parse messages in the
    HTTP/1 wire format. It allocates no memory and uses minimal
    state. It will handle chunked encoding and it understands the
    semantics of the Connection, Content-Length, and Upgrade
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
        // Called first when the Request Line is received.
        // Only called when isRequest == true.
        //
        void
        on_begin_request(
            boost::string_ref const& method,
            boost::string_ref const& path,
            int version,
            error_code& ec);

        // Called first when the Status Line is received
        // Only called when isRequest == false.
        //
        void
        on_begin_response(
            int status,
            boost::string_ref const& reason,
            int version,
            error_code& ec);

        // Called once for each field/value pair in the header.
        //
        void
        on_field(
            boost::string_ref const& name,
            boost::string_ref const& value,
            error_code& ec);

        // Called once after the header is complete.
        //
        void
        on_end_header(error_code& ec);

        // Called once before the body, if any, is started.
        // This will only be called if the semantics of the
        // message indicate that a body exists, including
        // an indicated body of zero length.
        //
        void
        on_begin_body(error_code& ec);

        // Called at the beginning of each chunk and final
        // chunk, when Transfer-Encoding specifies chunked.
        //
        void
        on_chunk(
            std::uint64_t length,           // Length of this chunk
            boost::string_ref const& ext,   // The chunk extensions, if any
            error_code& ec)
        {
        }

        // Called zero or more times to provide body data.
        //
        void
        on_body(
            boost::string_ref const& data,
            error_code& ec);

        // Called once after the full body is received.
        // This will only be called if the semantics of the
        // message indicate that a body exists, including
        // an indicated body of zero length.
        //
        void
        on_end_body(error_code& ec);

        // Called once when the message is complete.
        // This will be called even if there is no body.
        //
        void
        on_end_message(error_code& ec);
    };
    @endcode

    If a callback sets the error code, the error will be propagated
    to the caller of the parser. Behavior of parsing after an error
    is returned is undefined.

    Callbacks must not throw exceptions.

    @tparam isRequest A `bool` indicating whether the parser will be
    presented with request or response message.

    @tparam Derived The derived class type. This is part of the
    Curiously Recurring Template Pattern interface.
*/
template<bool isRequest, class Derived>
class basic_parser
    : private detail::basic_parser_base
{
    template<bool OtherIsRequest, class OtherDerived>
    friend class basic_parser;

    // Consider this message as having no body
    static unsigned constexpr flagSkipBody              = 1<<  0;

    // Parser will pause after reading the header
    static unsigned constexpr flagPauseBody             = 1<<  1;

    // Parser will pause after reading the header
    static unsigned constexpr flagSplitParse            = 1<<  2;

    // The parser has read at least one byte
    static unsigned constexpr flagGotSome               = 1<<  3;

    // Message semantics indicate a body is expected.
    // cleared if flagOmitBody set
    //
    static unsigned constexpr flagHasBody               = 1<<  4;

    // on_begin_body was called
    static unsigned constexpr flagBeginBody             = 1<<  5;

    static unsigned constexpr flagMsgDone               = 1<<  6;
    static unsigned constexpr flagGotHeader             = 1<<  7;
    static unsigned constexpr flagHTTP11                = 1<<  8;
    static unsigned constexpr flagNeedEOF               = 1<<  9;
    static unsigned constexpr flagExpectCRLF            = 1<< 10;
    static unsigned constexpr flagFinalChunk            = 1<< 11;

    static unsigned constexpr flagConnectionClose       = 1<< 12;
    static unsigned constexpr flagConnectionUpgrade     = 1<< 13;
    static unsigned constexpr flagConnectionKeepAlive   = 1<< 14;

    static unsigned constexpr flagContentLength         = 1<< 15;

    static unsigned constexpr flagChunked               = 1<< 16;

    static unsigned constexpr flagUpgrade               = 1<< 17;

    std::uint64_t len_;     // size of chunk or body
    char* buf_ = nullptr;
    std::size_t buf_len_ = 0;
    std::size_t skip_ = 0;  // search from here
    std::size_t x_;         // scratch variable
    unsigned f_ = 0;        // flags

protected:
    /// Default constructor
    basic_parser() = default;

public:
    /// `true` if this parser parses requests, `false` for responses.
    static bool constexpr is_request = isRequest;

    /// Copy constructor (disallowed).
    basic_parser(basic_parser const&) = delete;

    /// Copy assignment (disallowed).
    basic_parser& operator=(basic_parser const&) = delete;

    /// Destructor
    ~basic_parser();

    /** Move constructor

        After the move, the only valid operation on the
        moved-from object is destruction.
    */
    template<class OtherDerived>
    basic_parser(basic_parser<isRequest, OtherDerived>&&);

    /** Set options on the parser.

        @param args One or more parser options to set.
    */
#if GENERATING_DOCS
    template<class... Args>
    void
    set_option(Args&&... args)
#else
    template<class A1, class A2, class... An>
    void
    set_option(A1&& a1, A2&& a2, An&&... an)
#endif
    {
        set_option(std::forward<A1>(a1));
        set_option(std::forward<A2>(a2),
            std::forward<An>(an)...);
    }

    /// Set the @ref body_max_size option
    void
    set_option(body_max_size const& o)
    {
        // VFALCO TODO
    }

    /// Set the @ref header_max_size option
    void
    set_option(header_max_size const& o)
    {
        // VFALCO TODO
    }

    /// Set the @ref skip_body option.
    void
    set_option(skip_body const& opt);

    /** Returns `true` if the parser requires additional input.

        When this function returns `true`, the caller should
        perform one of the following actions in order for the
        parser to make forward progress:

        @li Commit additional bytes to the stream buffer, then
        call @ref write.

        @li Call @ref write_eof to indicate that the stream
        will never produce additional input.
    */
    bool
    need_more() const;

    /** Returns `true` if the message end is indicated by eof.

        This function returns `true` if the semantics of the message
        require that the end of the message is signaled by an end
        of file. For example, if the message is a HTTP/1.0 message
        and the Content-Length is unspecified, the end of the message
        is indicated by an end of file.

        @note The return value is undefined unless a complete
        header has been parsed.
    */
    bool
    need_eof() const
    {
        BOOST_ASSERT(got_header());
        return (f_ & flagNeedEOF) != 0;
    }

    /** Returns `true` if the parser is finished with the message.

        The message is finished when @ref got_header returns
        `true` is parsed and one of the following is true:

        @li The @ref skip_body option is set

        @li The semantics of the message indicate there is no body.

        @li The complete, expected body was parsed.
    */
    // VFALCO Is this function needed?
    bool
    is_done() const
    {
        return (f_ & flagMsgDone) != 0;
    }

    /// Returns `true` if the complete header has been parsed.
    bool
    got_header() const
    {
        return (f_ & flagGotHeader) != 0;
    }

    /** Returns `true` if the message is an upgrade message.

        @note The return value is undefined unless
            @ref got_header would return `true`.
    */
    bool
    is_upgrade() const
    {
        return (f_ & flagConnectionUpgrade) != 0;
    }

    /** Returns `true` if keep-alive is specified

        @note The return value is undefined unless
            @ref got_header would return `true`.
    */
    bool
    is_keep_alive() const;

    /** Returns `true` if the chunked Transfer-Encoding is specified.

        @note The return value is undefined unless
            @ref got_header would return `true`.
    */
    bool
    is_chunked() const
    {
        return (f_ & flagChunked) != 0;
    }

    /** Write a sequence of buffers to the parser.

        This function attempts to parse the HTTP message
        in the caller provided buffers. Upon success, a
        positive return value indicates that the parser
        made forward progress, consuming that number of
        bytes.

        A return value of zero indicates that the parser
        requires additional input. In this case the caller
        should append additional bytes to the input buffer
        sequence and call @ref write again.

        The function @ref need_more will return `true` in
        the case where additional bytes are needed for the
        parser to make forward progress.

        The parser is optimized for the case where the
        input buffer sequence consists of a single contiguous
        buffer. The @ref beast::flat_streambuf class is provided,
        which guarantees that the input sequence of the
        stream buffer will be represented by exactly one
        contiguous buffer. To ensure the optimum performance
        of the parser, use @ref beast::flat_streambuf with HTTP
        algorithms such as @ref beast::http::read,
        @ref beast::http::async_read, @ref beast::http::parse,
        and @ref beast::http::async_parse. Alternatively,
        the caller may use custom techniques to ensure that
        the structured portion of the HTTP message (header
        and chunk preambles if chunked) is contained in a
        linear buffer.

        @param buffers An object meeting the requirements of
        @b ConstBufferSequence that represents the message.

        @param ec Set to the error, if any occurred.

        @return The number of bytes consumed in the buffer
        sequence.
    */
    template<class ConstBufferSequence>
    std::size_t
    write(ConstBufferSequence const& buffers, error_code& ec);

    /** Write a single contiguous buffer to the parser.

        This function attempts to parse the HTTP message
        in the caller provided buffer. Upon success, a
        positive return value indicates that the parser
        made forward progress, consuming that number of
        bytes.

        A return value of zero indicates that the parser
        requires additional input. In this case the caller
        should append additional bytes to the input buffer
        and call @ref write again.

        The function @ref need_more will return `true` in
        the case where additional bytes are needed for the
        parser to make forward progress.

        The parser is optimized for the case where the
        input buffer sequence consists of a single linear
        buffer. This function exists for when the caller
        wants to take over the management of memory for
        the incoming bytes and ensure optimum parser
        performance. Alternatively, callers can use the
        @ref beast::flat_streambuf class with HTTP algorithms
        such as @ref beast::http::read,
        @ref beast::http::async_read, @ref beast::http::parse,
        and @ref beast::http::async_parse.

        @param buffer A buffer that contains the input
        sequence to be parsed.

        @param ec Set to the error, if any occurred.

        @return The number of bytes consumed in the input
        buffer.
    */
    std::size_t
    write(boost::asio::const_buffers_1 const& buffer,
        error_code& ec);

    /** Inform the parser that the end of file was reached.

        HTTP needs to know where the end of the stream is. For
        example, sometimes servers send responses without
        Content-Length and expect the client to consume input
        (for the body) until EOF. Callbacks and errors will still
        be processed as usual.

        @note This is typically called when a socket read returns
        the end of file error.

        @param ec Set to the error, if any occurred. If no bytes
        have been received by the parser when this function is
        called, `ec` will be set to `boost::asio::error::eof`.
    */
    void
    write_eof(error_code& ec);




    // VFALCO Stuff below here is in flux

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

    /** Returns the number of body bytes remaining in this chunk.
    */
    std::uint64_t
    remain() const
    {
        BOOST_ASSERT(got_header());
        if(f_ & (flagContentLength | flagChunked))
            return len_;
        // VFALCO This is ugly
        return 65536;
    }

    /** Transfer body octets from buffer to the reader
    */
    template<class Reader, class DynamicBuffer>
    void
    write_body(Reader& r,
        DynamicBuffer& dynabuf, error_code& ec);

    /** Consume body bytes from the current chunk.
    */
    void
    consume(std::uint64_t n)
    {
        len_ -= n;
    }

protected:
    /** Set the split parse option.

        When the derived class enables the split parse,
        the function @ref need_more will return `false`
        when a complete header has been received.
    */
    void
    split(bool value);

private:
    inline
    Derived&
    impl()
    {
        return *static_cast<Derived*>(this);
    }

    template<class ConstBufferSequence>
    boost::string_ref
    maybe_flatten(
        ConstBufferSequence const& buffers);

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
        boost::string_ref const& name,
            boost::string_ref const& value,
                error_code& ec);

    std::size_t
    parse_header(char const* p,
        std::size_t n, error_code& ec);

    void
    do_header(int, std::true_type);

    void
    do_header(int status, std::false_type);

    std::size_t
    parse_body(char const* p,
        std::size_t n, error_code& ec);

    std::size_t
    parse_chunk(char const* p,
        std::size_t n, error_code& ec);

    void
    do_end_message(error_code& ec);
};

} // http
} // beast

#include <beast/http/impl/basic_parser.ipp>

#endif
