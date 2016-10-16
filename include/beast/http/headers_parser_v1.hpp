//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_HEADERS_PARSER_V1_HPP
#define BEAST_HTTP_HEADERS_PARSER_V1_HPP

#include <beast/http/basic_parser_v1.hpp>
#include <beast/http/concepts.hpp>
#include <beast/http/message.hpp>
#include <beast/core/error.hpp>
#include <boost/assert.hpp>
#include <boost/optional.hpp>
#include <string>
#include <type_traits>
#include <utility>

namespace beast {
namespace http {

namespace detail {

struct request_parser_base
{
    std::string method_;
    std::string uri_;
};

struct response_parser_base
{
    std::string reason_;
};

} // detail

/** A parser for HTTP/1 request and response headers.

    This class uses the HTTP/1 wire format parser to
    convert a series of octets into a @ref request_headers
    or @ref @response_headers.

    @note A new instance of the parser is required for each message.
*/
template<bool isRequest, class Headers>
class headers_parser_v1
    : public basic_parser_v1<isRequest,
        headers_parser_v1<isRequest, Headers>>
    , private std::conditional<isRequest,
        detail::request_parser_base,
            detail::response_parser_base>::type
{
public:
    /// The type of message this parser produces.
    using headers_type =
        message_headers<isRequest, Headers>;

private:
    // VFALCO Check Headers requirements?

    std::string field_;
    std::string value_;
    headers_type h_;
    bool flush_ = false;

public:
    /// Default constructor
    headers_parser_v1() = default;

    /// Move constructor
    headers_parser_v1(headers_parser_v1&&) = default;

    /// Copy constructor (disallowed)
    headers_parser_v1(headers_parser_v1 const&) = delete;

    /// Move assignment (disallowed)
    headers_parser_v1& operator=(headers_parser_v1&&) = delete;

    /// Copy assignment (disallowed)
    headers_parser_v1& operator=(headers_parser_v1 const&) = delete;

    /** Construct the parser.

        @param args Forwarded to the message headers constructor.
    */
#if GENERATING_DOCS
    template<class... Args>
    explicit
    headers_parser_v1(Args&&... args);
#else
    template<class Arg1, class... ArgN,
        class = typename std::enable_if<! std::is_same<
            typename std::decay<Arg1>::type, headers_parser_v1>::value>>
    explicit
    headers_parser_v1(Arg1&& arg1, ArgN&&... argn)
        : h_(std::forward<Arg1>(arg1),
            std::forward<ArgN>(argn)...)
    {
    }
#endif

    /** Returns the parsed headers.

        Only valid if @ref complete would return `true`.
    */
    headers_type const&
    get() const
    {
        return h_;
    }

    /** Returns the parsed headers.

        Only valid if @ref complete would return `true`.
    */
    headers_type&
    get()
    {
        return h_;
    }

    /** Returns ownership of the parsed headers.

        Ownership is transferred to the caller. Only
        valid if @ref complete would return `true`.

        Requires:
            `message_headers<isRequest, Headers>` is @b MoveConstructible
    */
    headers_type
    release()
    {
        static_assert(std::is_move_constructible<decltype(h_)>::value,
            "MoveConstructible requirements not met");
        return std::move(h_);
    }

private:
    friend class basic_parser_v1<isRequest, headers_parser_v1>;

    void flush()
    {
        if(! flush_)
            return;
        flush_ = false;
        BOOST_ASSERT(! field_.empty());
        h_.headers.insert(field_, value_);
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

    void on_request_or_response(std::true_type)
    {
        h_.method = std::move(this->method_);
        h_.url = std::move(this->uri_);
    }

    void on_request_or_response(std::false_type)
    {
        h_.status = this->status_code();
        h_.reason = std::move(this->reason_);
    }

    void on_request(error_code& ec)
    {
        on_request_or_response(
            std::integral_constant<bool, isRequest>{});
    }

    void on_response(error_code& ec)
    {
        on_request_or_response(
            std::integral_constant<bool, isRequest>{});
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

    void
    on_headers(std::uint64_t, error_code&)
    {
        flush();
        h_.version = 10 * this->http_major() + this->http_minor();
    }

    body_what
    on_body_what(std::uint64_t, error_code&)
    {
        return body_what::pause;
    }

    void on_body(boost::string_ref const&, error_code&)
    {
    }

    void on_complete(error_code&)
    {
    }
};

} // http
} // beast

#endif
