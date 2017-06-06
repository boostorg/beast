//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_MESSAGE_IPP
#define BEAST_HTTP_IMPL_MESSAGE_IPP

#include <beast/core/error.hpp>
#include <beast/http/rfc7230.hpp>
#include <beast/core/detail/ci_char_traits.hpp>
#include <beast/core/detail/type_traits.hpp>
#include <boost/assert.hpp>
#include <stdexcept>

namespace beast {
namespace http {

template<class Fields>
template<class>
string_view
header<true, Fields>::
get_method_string() const
{
    if(method_ != verb::unknown)
        return to_string(method_);
    return this->method_impl();
}

template<class Fields>
template<class>
void
header<true, Fields>::
set_method(verb v)
{
    if(v == verb::unknown)
        BOOST_THROW_EXCEPTION(
            std::invalid_argument{"unknown verb"});
    method_ = v;
    this->method_impl({});
}

template<class Fields>
template<class>
void
header<true, Fields>::
set_method(string_view s)
{
    method_ = string_to_verb(s);
    if(method_ != verb::unknown)
        this->method_impl({});
    else
        this->method_impl(s);
}

template<class Fields>
template<class>
string_view
header<false, Fields>::
get_reason() const
{
    auto const s = this->reason_impl();
    if(! s.empty())
        return s;
    return obsolete_reason(result_);
}

//------------------------------------------------------------------------------

namespace detail {

} // detail

template<bool isRequest, class Body, class Fields>
bool
message<isRequest, Body, Fields>::
chunked() const
{
    auto const it0 = this->find("Transfer-Encoding");
    if(it0 == this->end())
        return false;
    token_list value{*it0};
    for(auto it = value.begin(); it != value.end();)
    {
        auto cur = it++;
        if(it == value.end())
            return *cur == "chunked";
    }
    return false;
}

template<bool isRequest, class Body, class Fields>
boost::optional<std::uint64_t>
message<isRequest, Body, Fields>::
size() const
{
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");

    return size(detail::is_body_sized<
        Body, decltype(*this)>{});
}

template<bool isRequest, class Body, class Fields>
template<class... Args>
void
message<isRequest, Body, Fields>::
prepare(Args const&... args)
{
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");

    unsigned f = 0;
    prepare_opt(f, args...);

    if(f & 1)
    {
        if(this->version > 10)
            this->connection_impl(connection::close);
    }

    if(f & 2)
    {
        if(this->version < 11)
            this->connection_impl(connection::keep_alive);
    }

    if(f & 4)
    {
        if(this->version < 11)
            BOOST_THROW_EXCEPTION(std::invalid_argument{
                "invalid connection upgrade"});
        this->connection_impl(connection::upgrade);
    }

    prepare_payload(typename header<
        isRequest, Fields>::is_request{});
}

template<bool isRequest, class Body, class Fields>
template<class Arg, class... Args>
inline
void
message<isRequest, Body, Fields>::
prepare_opt(unsigned& f,
    Arg const& arg, Args const&... args)
{
    prepare_opt(f, arg);
    prepare_opt(f, args...);
}

template<bool isRequest, class Body, class Fields>
inline
void
message<isRequest, Body, Fields>::
prepare_opt(unsigned& f, close_t)
{
    f |= 1;
}

template<bool isRequest, class Body, class Fields>
inline
void
message<isRequest, Body, Fields>::
prepare_opt(unsigned& f, keep_alive_t)
{
    f |= 2;
}

template<bool isRequest, class Body, class Fields>
inline
void
message<isRequest, Body, Fields>::
prepare_opt(unsigned& f, upgrade_t)
{
    f |= 4;
}

template<bool isRequest, class Body, class Fields>
inline
void
message<isRequest, Body, Fields>::
prepare_payload(std::true_type)
{
    auto const n = size();
    if(this->method_ == verb::trace &&
            (! n || *n > 0))
        BOOST_THROW_EXCEPTION(std::invalid_argument{
            "invalid request body"});
    if(n)
    {
        if(*n > 0 ||
            this->method_ == verb::options ||
            this->method_ == verb::put ||
            this->method_ == verb::post
            )
        {
            this->content_length_impl(*n);
        }
    }
    else if(this->version >= 11)
    {
        this->chunked_impl();
    }
}

template<bool isRequest, class Body, class Fields>
inline
void
message<isRequest, Body, Fields>::
prepare_payload(std::false_type)
{
    auto const n = size();
    if((status_class(this->result_) ==
            status_class::informational ||
        this->result_ == status::no_content ||
        this->result_ == status::not_modified))
    {
        if(! n || *n > 0)
            // The response body MUST BE empty for this case
            BOOST_THROW_EXCEPTION(std::invalid_argument{
                "invalid response body"});
    }
    if(n)
        this->content_length_impl(*n);
    else if(this->version >= 11)
        this->chunked_impl();
}

//------------------------------------------------------------------------------

template<class Fields>
void
swap(
    header<true, Fields>& h1,
    header<true, Fields>& h2)
{
    using std::swap;
    swap(
        static_cast<Fields&>(h1),
        static_cast<Fields&>(h2));
    swap(h1.version, h2.version);
    swap(h1.method_, h2.method_);
}

template<class Fields>
void
swap(
    header<false, Fields>& h1,
    header<false, Fields>& h2)
{
    using std::swap;
    swap(
        static_cast<Fields&>(h1),
        static_cast<Fields&>(h2));
    swap(h1.version, h2.version);
    swap(h1.result_, h2.result_);
}

template<bool isRequest, class Body, class Fields>
void
swap(
    message<isRequest, Body, Fields>& m1,
    message<isRequest, Body, Fields>& m2)
{
    using std::swap;
    swap(
        static_cast<header<isRequest, Fields>&>(m1),
        static_cast<header<isRequest, Fields>&>(m2));
    swap(m1.body, m2.body);
}

template<bool isRequest, class Fields>
bool
is_keep_alive(header<isRequest, Fields> const& msg)
{
    BOOST_ASSERT(msg.version == 10 || msg.version == 11);
    if(msg.version == 11)
    {
        if(token_list{msg["Connection"]}.exists("close"))
            return false;
        return true;
    }
    if(token_list{msg["Connection"]}.exists("keep-alive"))
        return true;
    return false;
}

template<bool isRequest, class Fields>
bool
is_upgrade(header<isRequest, Fields> const& msg)
{
    BOOST_ASSERT(msg.version == 10 || msg.version == 11);
    if(msg.version == 10)
        return false;
    if(token_list{msg["Connection"]}.exists("upgrade"))
        return true;
    return false;
}

} // http
} // beast

#endif
