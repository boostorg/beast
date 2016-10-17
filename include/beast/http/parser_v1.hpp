//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_PARSER_V1_HPP
#define BEAST_HTTP_PARSER_V1_HPP

#include <beast/http/basic_parser_v1.hpp>
#include <beast/http/concepts.hpp>
#include <beast/http/message_v1.hpp>
#include <beast/core/error.hpp>
#include <beast/core/detail/ignore_unused.hpp>
#include <boost/assert.hpp>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

namespace beast {
namespace http {

namespace detail {

struct parser_request
{
    std::string method_;
    std::string uri_;
};

struct parser_response
{
    std::string reason_;
};

} // detail

/** Skip body option.

    The options controls whether or not the parser expects to see a
    HTTP body, regardless of the presence or absence of certain fields
    such as Content-Length.

    Depending on the request, some responses do not carry a body.
    For example, a 200 response to a CONNECT request from a tunneling
    proxy. In these cases, callers use the @ref skip_body option to
    inform the parser that no body is expected. The parser will consider
    the message complete after the all headers have been received.

    Example:
    @code
        parser_v1<true, empty_body, headers> p;
        p.set_option(skip_body{true});
    @endcode

    @note Objects of this type are passed to @ref basic_parser_v1::set_option.
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

/** A parser for producing HTTP/1 messages.

    This class uses the basic HTTP/1 wire format parser to convert
    a series of octets into a `message_v1`.

    @note A new instance of the parser is required for each message.
*/
template<bool isRequest, class Body, class Headers>
class parser_v1
    : public basic_parser_v1<isRequest,
        parser_v1<isRequest, Body, Headers>>
    , private std::conditional<isRequest,
        detail::parser_request, detail::parser_response>::type
{
public:
    /// The type of message this parser produces.
    using message_type =
        message_v1<isRequest, Body, Headers>;

private:
    static_assert(is_ReadableBody<Body>::value,
        "ReadableBody requirements not met");

    std::string field_;
    std::string value_;
    message_type m_;
    typename message_type::body_type::reader r_;
    std::uint8_t skip_body_ = 0;
    bool flush_ = false;

public:
    /// Move constructor
    parser_v1(parser_v1&&) = default;

    /// Copy constructor (disallowed)
    parser_v1(parser_v1 const&) = delete;

    /// Move assignment (disallowed)
    parser_v1& operator=(parser_v1&&) = delete;

    /// Copy assignment (disallowed)
    parser_v1& operator=(parser_v1 const&) = delete;

    /// Default constructor
    parser_v1()
        : r_(m_)
    {
    }

    /** Construct the parser.

        @param args A list of arguments forwarded to the message constructor.
    */
    template<class Arg1, class... ArgN,
        class = typename std::enable_if<! std::is_same<
            typename std::decay<Arg1>::type, parser_v1>::value>>
    explicit
    parser_v1(Arg1&& arg1, ArgN&&... argn)
        : m_(std::forward<Arg1>(arg1),
            std::forward<ArgN>(argn)...)
        , r_(m_)
    {
    }

    /// Set the expect body option.
    void
    set_option(skip_body const& o)
    {
        skip_body_ = o.value ? 1 : 0;
    }

    /** Returns the parsed message.

        Only valid if `complete()` would return `true`.
    */
    message_type const&
    get() const
    {
        return m_;
    }

    /** Returns the parsed message.

        Only valid if `complete()` would return `true`.
    */
    message_type&
    get()
    {
        return m_;
    }

    /** Returns the parsed message.

        Ownership is transferred to the caller.
        Only valid if `complete()` would return `true`.

        Requires:
            `message<isRequest, Body, Headers>` is MoveConstructible
    */
    message_type
    release()
    {
        static_assert(std::is_move_constructible<decltype(m_)>::value,
            "MoveConstructible requirements not met");
        return std::move(m_);
    }

private:
    friend class basic_parser_v1<isRequest, parser_v1>;

    void flush()
    {
        if(! flush_)
            return;
        flush_ = false;
        BOOST_ASSERT(! field_.empty());
        m_.headers.insert(field_, value_);
        field_.clear();
        value_.clear();
    }

    void on_start(error_code&)
    {
    }

    void on_method(boost::string_ref const& s, error_code&)
    {
        this->method_.append(s.data(), s.size());
    }

    void on_uri(boost::string_ref const& s, error_code&)
    {
        this->uri_.append(s.data(), s.size());
    }

    void on_reason(boost::string_ref const& s, error_code&)
    {
        this->reason_.append(s.data(), s.size());
    }

    void on_field(boost::string_ref const& s, error_code&)
    {
        flush();
        field_.append(s.data(), s.size());
    }

    void on_value(boost::string_ref const& s, error_code&)
    {
        value_.append(s.data(), s.size());
        flush_ = true;
    }

    void set(std::true_type)
    {
        m_.method = std::move(this->method_);
        m_.url = std::move(this->uri_);
    }

    void set(std::false_type)
    {
        m_.status = this->status_code();
        m_.reason = std::move(this->reason_);
    }

    body_what
    on_headers(std::uint64_t, error_code&)
    {
        flush();
        m_.version = 10 * this->http_major() + this->http_minor();
        if(skip_body_)
            return body_what::skip;
        return body_what::normal;
    }

    void on_request(error_code& ec)
    {
        beast::detail::ignore_unused(ec);
        set(std::integral_constant<
            bool, isRequest>{});
    }

    void on_response(error_code& ec)
    {
        beast::detail::ignore_unused(ec);
        set(std::integral_constant<
            bool, isRequest>{});
    }

    void on_body(boost::string_ref const& s, error_code& ec)
    {
        r_.write(s.data(), s.size(), ec);
    }

    void on_complete(error_code&)
    {
    }
};

} // http
} // beast

#endif
