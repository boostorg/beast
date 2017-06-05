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

/** An HTTP/1 parser for producing a message.

    This class uses the basic HTTP/1 wire format parser to convert
    a series of octets into a @ref message.

    @tparam isRequest Indicates whether a request or response
    will be parsed.

    @tparam Body The type used to represent the body.

    @tparam Fields The type of container used to represent the fields.

    @note A new instance of the parser is required for each message.
*/
template<
    bool isRequest,
    class Body,
    class Fields = fields>
class parser
    : public basic_parser<isRequest,
        parser<isRequest, Body, Fields>>
{
    static_assert(is_body<Body>::value,
        "Body requirements not met");

    static_assert(is_body_writer<Body>::value,
        "BodyWriter requirements not met");

    template<bool, class, class>
    friend class parser;

    using base_type = basic_parser<isRequest,
        parser<isRequest, Body, Fields>>;

    message<isRequest, Body, Fields> m_;
    boost::optional<typename Body::writer> wr_;

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
    parser(parser&& other) = default;

    /** Constructor

        @param args Optional arguments forwarded to the 
        @ref http::header constructor.

        @note This function participates in overload
        resolution only if the first argument is not a
        @ref parser.
    */
#if BEAST_DOXYGEN
    template<class... Args>
    explicit
    msesage_parser(Args&&... args);
#else
    template<class Arg1, class... ArgN,
        class = typename std::enable_if<
            ! detail::is_parser<typename
                std::decay<Arg1>::type>::value>::type>
    explicit
    parser(Arg1&& arg1, ArgN&&... argn);
#endif

    /** Construct a parser from another parser, changing the Body type.

        This constructs a new parser by move constructing the
        header from another parser with a different body type. The
        constructed-from parser must not have any parsed body octets or
        initialized @b BodyWriter, otherwise an exception is generated.

        @par Example
        @code
        // Deferred body type commitment
        request_parser<empty_body> req0;
        ...
        request_parser<string_body> req{std::move(req0)};
        @endcode

        If an exception is thrown, the state of the constructed-from
        parser is undefined.

        @param parser The other parser to construct from. After
        this call returns, the constructed-from parser may only
        be destroyed.

        @param args Optional arguments forwarded to the message
        constructor.

        @throws std::invalid_argument Thrown when the constructed-from
        parser has already initialized a body writer.
    */
#if BEAST_DOXYGEN
    template<class OtherBody, class... Args>
#else
    template<class OtherBody, class... Args,
        class = typename std::enable_if<
            ! std::is_same<Body, OtherBody>::value>::type>
#endif
    explicit
    parser(parser<isRequest, OtherBody, Fields>&& parser,
        Args&&... args);

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
    friend class basic_parser<isRequest, parser>;

    void
    on_request(verb method, string_view method_str,
        string_view target, int version, error_code&)
    {
        m_.target(target);
        if(method != verb::unknown)
            m_.method(method);
        else
            m_.method(method_str);
        m_.version = version;
    }

    void
    on_response(int code,
        string_view reason,
            int version, error_code&)
    {
        m_.result(code);
        m_.version = version;
        m_.reason(reason);
    }

    void
    on_field(string_view name,
        string_view value,
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
    on_data(string_view s,
        error_code& ec)
    {
        wr_->put(boost::asio::buffer(
            s.data(), s.size()), ec);
    }

    void
    on_chunk(
        std::uint64_t, string_view,
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
