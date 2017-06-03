//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_PARSER_HPP
#define BEAST_HTTP_PARSER_HPP

#include <beast/config.hpp>
#include <beast/http/basic_parser.hpp>
#include <beast/http/message.hpp>
#include <beast/http/type_traits.hpp>
#include <boost/optional.hpp>
#include <boost/throw_exception.hpp>
#include <type_traits>
#include <utility>

namespace beast {
namespace http {

/** An HTTP/1 parser for producing a header.

    This class uses the basic HTTP/1 wire format parser to convert
    a series of octets into a @ref header.

    @note A new instance of the parser is required for each message.

    @tparam isRequest Indicates whether a request or response
    will be parsed.

    @tparam Fields The type of container used to represent the fields.
*/
template<bool isRequest, class Fields>
class header_parser
    : public basic_parser<isRequest,
        header_parser<isRequest, Fields>>
{
    header<isRequest, Fields> h_;
    string_view body_;

public:
    /// The type of @ref header this object produces.
    using value_type = header<isRequest, Fields>;

    /// Default constructor.
    header_parser() = default;

    /// Copy constructor.
    header_parser(header_parser const&) = default;

    /// Copy assignment.
    header_parser& operator=(header_parser const&) = default;

    /** Move constructor.

        After the move, the only valid operation
        on the moved-from object is destruction.
    */
    header_parser(header_parser&&) = default;

    /** Move assignment

        After the move, the only valid operation
        on the moved-from object is destruction.
    */
    header_parser& operator=(header_parser&&) = default;

    /** Constructor

        @param args Optional arguments forwarded
        forwarded to the @ref http::header constructor.
    */
#if BEAST_DOXYGEN
    template<class... Args>
    explicit
    header_parser(Args&&... args);
#else
    template<class Arg0, class... ArgN,
        class = typename std::enable_if<
        ! std::is_convertible<typename
            std::decay<Arg0>::type,
                header_parser>::value>>
    explicit
    header_parser(Arg0&& arg0, ArgN&&... argn);
#endif

    /** Returns parsed body octets.

        This function will return the most recent buffer
        of octets corresponding to the parsed body. This
        buffer will become invalidated on any subsequent
        call to @ref put or @ref put_eof
    */
    string_view
    body() const
    {
        return body_;
    }

    /** Returns the parsed header

        @note The return value is undefined unless
        @ref is_header_done would return `true`.
    */
    value_type const&
    get() const
    {
        return h_;
    }

    /** Returns the parsed header.

        @note The return value is undefined unless
        @ref is_header_done would return `true`.
    */
    value_type&
    get()
    {
        return h_;
    }

    /** Returns ownership of the parsed header.

        Ownership is transferred to the caller.

        @note The return value is undefined unless
        @ref is_header_done would return `true`.

        Requires:
            @ref value_type is @b MoveConstructible
    */
    value_type
    release()
    {
        static_assert(
            std::is_move_constructible<decltype(h_)>::value,
            "MoveConstructible requirements not met");
        return std::move(h_);
    }

private:
    friend class basic_parser<isRequest, header_parser>;

    void
    on_request(string_view method,
        string_view path, int version, error_code&)
    {
        h_.target(path);
        h_.method(method);
        h_.version = version;
    }

    void
    on_response(int status, string_view reason,
        int version, error_code&)
    {
        h_.status = status;
        h_.version = version;
        h_.reason(reason);
    }

    void
    on_field(string_view name,
        string_view value, error_code&)
    {
        h_.fields.insert(name, value);
    }

    void
    on_header(error_code&)
    {
    }

    void
    on_body(boost::optional<std::
        uint64_t> const&, error_code&)
    {
    }

    void
    on_data(string_view s, error_code&)
    {
        body_ = s;
    }

    void
    on_chunk(std::uint64_t,
        string_view const&, error_code&)
    {
        body_ = {};
    }

    void
    on_complete(error_code&)
    {
        body_ = {};
    }
};

/** An HTTP/1 parser for producing a message.

    This class uses the basic HTTP/1 wire format parser to convert
    a series of octets into a @ref message.

    @tparam isRequest Indicates whether a request or response
    will be parsed.

    @tparam Body The type used to represent the body.

    @tparam Fields The type of container used to represent the fields.

    @note A new instance of the parser is required for each message.
*/
template<bool isRequest, class Body, class Fields = fields>
class parser
    : public basic_parser<isRequest,
        parser<isRequest, Body, Fields>>
{
    static_assert(is_body<Body>::value,
        "Body requirements not met");

    static_assert(is_body_writer<Body>::value,
        "BodyWriter requirements not met");

    using base_type = basic_parser<isRequest,
        parser<isRequest, Body, Fields>>;

    using writer_type = typename Body::writer;

    message<isRequest, Body, Fields> m_;
    boost::optional<writer_type> wr_;

public:
    /// The type of message returned by the parser
    using value_type = message<isRequest, Body, Fields>;

    /// Constructor (default)
    parser() = default;

    /// Copy constructor (disallowed)
    parser(parser const&) = delete;

    /// Copy assignment (disallowed)
    parser& operator=(parser const&) = delete;

    /** Move constructor.

        After the move, the only valid operation
        on the moved-from object is destruction.
    */
    parser(parser&& other);

    /** Constructor

        @param args Optional arguments forwarded to the 
        @ref http::header constructor.

        @note This function participates in overload
        resolution only if the first argument is not a
        @ref http::header_parser or @ref parser.
    */
#if BEAST_DOXYGEN
    template<class... Args>
    explicit
    msesage_parser(Args&&... args);
#else
    template<class Arg1, class... ArgN,
        class = typename std::enable_if<
            ! std::is_same<typename
                std::decay<Arg1>::type,
                    header_parser<isRequest, Fields>>::value &&
            ! std::is_same<typename
                std::decay<Arg1>::type, parser>::value
                    >::type>
    explicit
    parser(Arg1&& arg1, ArgN&&... argn);
#endif

    /** Construct a message parser from a @ref header_parser.
        @param parser The header parser to construct from.
        @param args Optional arguments forwarded to the message
        constructor.
    */
    template<class... Args>
    explicit
    parser(header_parser<
        isRequest, Fields>&& parser, Args&&... args);

    /** Returns the parsed message.

        Depending on the progress of the parser, portions
        of this object may be incomplete.
    */
    value_type const&
    get() const
    {
        return m_;
    }

    /** Returns the parsed message.

        Depending on the progress of the parser, portions
        of this object may be incomplete.
    */
    value_type&
    get()
    {
        return m_;
    }

    /** Returns ownership of the parsed message.

        Ownership is transferred to the caller.
        Depending on the progress of the parser, portions
        of this object may be incomplete.

        @par Requires

        @ref value_type is @b MoveConstructible
    */
    value_type
    release()
    {
        static_assert(std::is_move_constructible<decltype(m_)>::value,
            "MoveConstructible requirements not met");
        return std::move(m_);
    }

private:
    friend class basic_parser<
        isRequest, parser>;

    void
    on_request(
        string_view const& method,
            string_view const& target,
                int version, error_code&)
    {
        m_.target(target);
        m_.method(method);
        m_.version = version;
    }

    void
    on_response(int status,
        string_view const& reason,
            int version, error_code&)
    {
        m_.status = status;
        m_.version = version;
        m_.reason(reason);
    }

    void
    on_field(string_view const& name,
        string_view const& value,
            error_code&)
    {
        m_.fields.insert(name, value);
    }

    void
    on_header(error_code& ec)
    {
    }

    void
    on_body(boost::optional<
        std::uint64_t> const& content_length,
            error_code& ec)
    {
        wr_.emplace(m_);
        wr_->init(content_length, ec);
    }

    void
    on_data(string_view const& s,
        error_code& ec)
    {
        wr_->put(boost::asio::buffer(
            s.data(), s.size()), ec);
    }

    void
    on_chunk(
        std::uint64_t, string_view const&,
            error_code&)
    {
    }

    void
    on_complete(error_code& ec)
    {
        if(wr_)
            wr_->finish(ec);
    }
};

/// An HTTP/1 parser for producing a request message.
template<class Body, class Fields = fields>
using request_parser = parser<true, Body, Fields>;

/// An HTTP/1 parser for producing a response message.
template<class Body, class Fields = fields>
using response_parser = parser<false, Body, Fields>;

} // http
} // beast

#include <beast/http/impl/parser.ipp>

#endif
