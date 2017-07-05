//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_MESSAGE_IPP
#define BEAST_HTTP_IMPL_MESSAGE_IPP

#include <beast/core/error.hpp>
#include <beast/core/detail/type_traits.hpp>
#include <boost/assert.hpp>
#include <boost/throw_exception.hpp>
#include <stdexcept>

namespace beast {
namespace http {

template<class Fields>
template<class Arg1, class... ArgN, class>
header<true, Fields>::
header(Arg1&& arg1, ArgN&&... argn)
    : Fields(std::forward<Arg1>(arg1),
        std::forward<ArgN>(argn)...)
{
}

template<class Fields>
inline
verb
header<true, Fields>::
method() const
{
    return method_;
}

template<class Fields>
void
header<true, Fields>::
method(verb v)
{
    if(v == verb::unknown)
        BOOST_THROW_EXCEPTION(
            std::invalid_argument{"unknown method"});
    method_ = v;
    this->set_method_impl({});
}

template<class Fields>
string_view
header<true, Fields>::
method_string() const
{
    if(method_ != verb::unknown)
        return to_string(method_);
    return this->get_method_impl();
}

template<class Fields>
void
header<true, Fields>::
method_string(string_view s)
{
    method_ = string_to_verb(s);
    if(method_ != verb::unknown)
        this->set_method_impl({});
    else
        this->set_method_impl(s);
}

template<class Fields>
inline
string_view
header<true, Fields>::
target() const
{
    return this->get_target_impl();
}

template<class Fields>
inline
void
header<true, Fields>::
target(string_view s)
{
    this->set_target_impl(s);
}

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

//------------------------------------------------------------------------------

template<class Fields>
template<class Arg1, class... ArgN, class>
header<false, Fields>::
header(Arg1&& arg1, ArgN&&... argn)
    : Fields(std::forward<Arg1>(arg1),
        std::forward<ArgN>(argn)...)
{
}

#if 0
template<class Fields>
template<class... Args>
header<false, Fields>::
header(status result, unsigned version_, Args&&... args)
    : Fields(std::forward<Args>(args)...)
    , version(version_)
    , result_(result)
{
}
#endif

template<class Fields>
inline
status
header<false, Fields>::
result() const
{
    return int_to_status(
        static_cast<int>(result_));
}

template<class Fields>
inline
void
header<false, Fields>::
result(status v)
{
    result_ = v;
}

template<class Fields>
inline
void
header<false, Fields>::
result(unsigned v)
{
    if(v > 999)
        BOOST_THROW_EXCEPTION(
            std::invalid_argument{
                "invalid status-code"});
    result_ = static_cast<status>(v);
}

template<class Fields>
inline
unsigned
header<false, Fields>::
result_int() const
{
    return static_cast<unsigned>(result_);
}

template<class Fields>
string_view
header<false, Fields>::
reason() const
{
    auto const s = this->get_reason_impl();
    if(! s.empty())
        return s;
    return obsolete_reason(result_);
}

template<class Fields>
inline
void
header<false, Fields>::
reason(string_view s)
{
    this->set_reason_impl(s);
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

//------------------------------------------------------------------------------

template<bool isRequest, class Body, class Fields>
template<class... BodyArgs>
message<isRequest, Body, Fields>::
message(header_type&& h, BodyArgs&&... body_args)
    : header_type(std::move(h))
    , body(std::forward<BodyArgs>(body_args)...)
{
}

template<bool isRequest, class Body, class Fields>
template<class... BodyArgs>
message<isRequest, Body, Fields>::
message(header_type const& h, BodyArgs&&... body_args)
    : header_type(h)
    , body(std::forward<BodyArgs>(body_args)...)
{
}

template<bool isRequest, class Body, class Fields>
template<class Version, class>
message<isRequest, Body, Fields>::
message(verb method, string_view target, Version version)
    : header_type(method, target, version)
{
}

template<bool isRequest, class Body, class Fields>
template<class Version, class BodyArg, class>
message<isRequest, Body, Fields>::
message(verb method, string_view target,
        Version version, BodyArg&& body_arg)
    : header_type(method, target, version)
    , body(std::forward<BodyArg>(body_arg))
{
}

template<bool isRequest, class Body, class Fields>
template<class Version, class BodyArg, class FieldsArg, class>
message<isRequest, Body, Fields>::
message(
    verb method, string_view target, Version version,
    BodyArg&& body_arg,
    FieldsArg&& fields_arg)
    : header_type(method, target, version,
        std::forward<FieldsArg>(fields_arg))
    , body(std::forward<BodyArg>(body_arg))
{
}

template<bool isRequest, class Body, class Fields>
template<class Version, class>
message<isRequest, Body, Fields>::
message(status result, Version version)
    : header_type(result, version)
{
}

template<bool isRequest, class Body, class Fields>
template<class Version, class BodyArg, class>
message<isRequest, Body, Fields>::
message(status result, Version version,
    BodyArg&& body_arg)
    : header_type(result, version)
    , body(std::forward<BodyArg>(body_arg))
{
}

template<bool isRequest, class Body, class Fields>
template<class Version, class BodyArg, class FieldsArg, class>
message<isRequest, Body, Fields>::
message(status result, Version version,
    BodyArg&& body_arg, FieldsArg&& fields_arg)
    : header_type(result, version,
        std::forward<FieldsArg>(fields_arg))
    , body(std::forward<BodyArg>(body_arg))
{
}

template<bool isRequest, class Body, class Fields>
message<isRequest, Body, Fields>::
message(std::piecewise_construct_t)
{
}

template<bool isRequest, class Body, class Fields>
template<class... BodyArgs>
message<isRequest, Body, Fields>::
message(std::piecewise_construct_t,
        std::tuple<BodyArgs...> body_args)
    : message(std::piecewise_construct,
        body_args,
        beast::detail::make_index_sequence<
            sizeof...(BodyArgs)>{})
{
}

template<bool isRequest, class Body, class Fields>
template<class... BodyArgs, class... FieldsArgs>
message<isRequest, Body, Fields>::
message(std::piecewise_construct_t,
    std::tuple<BodyArgs...> body_args,
    std::tuple<FieldsArgs...> fields_args)
    : message(std::piecewise_construct,
        body_args,
        fields_args,
        beast::detail::make_index_sequence<
            sizeof...(BodyArgs)>{},
        beast::detail::make_index_sequence<
            sizeof...(FieldsArgs)>{})
{
}

#if 0
template<bool isRequest, class Body, class Fields>
template<class BodyArg, class>
message<isRequest, Body, Fields>::
message(BodyArg&& body_arg)
    : body(std::forward<BodyArg>(body_arg))
{
}

template<bool isRequest, class Body, class Fields>
template<class BodyArg, class HeaderArg, class>
message<isRequest, Body, Fields>::
message(BodyArg&& body_arg, HeaderArg&& header_arg)
    : header_type(std::forward<HeaderArg>(header_arg))
    , body(std::forward<BodyArg>(body_arg))
{
}

template<bool isRequest, class Body, class Fields>
template<class... BodyArgs, class... HeaderArgs>
message<isRequest, Body, Fields>::
message(std::piecewise_construct_t,
    std::tuple<BodyArgs...>&& body_args,
        std::tuple<HeaderArgs...>&& header_args)
    : message(std::piecewise_construct,
        body_args, header_args,
            beast::detail::make_index_sequence<
                sizeof...(BodyArgs)>{},
                    beast::detail::make_index_sequence<
                        sizeof...(HeaderArgs)>{})
{
}
#endif

template<bool isRequest, class Body, class Fields>
boost::optional<std::uint64_t>
message<isRequest, Body, Fields>::
payload_size() const
{
    return payload_size(detail::is_body_sized<Body>{});
}

template<bool isRequest, class Body, class Fields>
void
message<isRequest, Body, Fields>::
prepare_payload(std::true_type)
{
    auto const n = payload_size();
    if(this->method() == verb::trace && (! n || *n > 0))
        BOOST_THROW_EXCEPTION(std::invalid_argument{
            "invalid request body"});
    if(n)
    {
        if(*n > 0 ||
            this->method() == verb::options ||
            this->method() == verb::put ||
            this->method() == verb::post)
        {
            this->prepare_payload_impl(false, *n);
        }
        else
        {
            this->prepare_payload_impl(false, boost::none);
        }
    }
    else if(this->version >= 11)
    {
        this->prepare_payload_impl(true, boost::none);
    }
    else
    {
        this->prepare_payload_impl(false, boost::none);
    }
}

template<bool isRequest, class Body, class Fields>
void
message<isRequest, Body, Fields>::
prepare_payload(std::false_type)
{
    auto const n = payload_size();
    if((status_class(this->result()) == status_class::informational ||
        this->result() == status::no_content ||
        this->result() == status::not_modified))
    {
        if(! n || *n > 0)
            // The response body MUST BE empty for this case
            BOOST_THROW_EXCEPTION(std::invalid_argument{
                "invalid response body"});
    }
    if(n)
    {
        this->prepare_payload_impl(false, *n);
    }
    else
    {
        this->prepare_payload_impl(true, boost::none);
    }
}

//------------------------------------------------------------------------------

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

} // http
} // beast

#endif
