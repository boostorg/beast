//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_IMPL_PARSER_IPP
#define BOOST_BEAST_HTTP_IMPL_PARSER_IPP

#include <boost/throw_exception.hpp>
#include <stdexcept>

namespace boost {
namespace beast {
namespace http {

template<bool isRequest, class Body, class Allocator>
parser<isRequest, Body, Allocator>::
parser()
    : parser{detail::has_deprecated_body_reader<Body>{}}
{
}

template<bool isRequest, class Body, class Allocator>
parser<isRequest, Body, Allocator>::
parser(std::true_type)
    : rd_(m_)
{
#ifndef BOOST_BEAST_ALLOW_DEPRECATED
    // Deprecated BodyReader Concept (v1.66)
    static_assert(sizeof(Body) == 0,
        BOOST_BEAST_DEPRECATION_STRING);
#endif
}

template<bool isRequest, class Body, class Allocator>
parser<isRequest, Body, Allocator>::
parser(std::false_type)
    : rd_(m_.base(), m_.body())
{
}

template<bool isRequest, class Body, class Allocator>
template<class Arg1, class... ArgN, class>
parser<isRequest, Body, Allocator>::
parser(Arg1&& arg1, ArgN&&... argn)
    : parser(std::forward<Arg1>(arg1),
        detail::has_deprecated_body_reader<Body>{},
        std::forward<ArgN>(argn)...)
{
}

// VFALCO arg1 comes before `true_type` to make
//        the signature unambiguous.
template<bool isRequest, class Body, class Allocator>
template<class Arg1, class... ArgN, class>
parser<isRequest, Body, Allocator>::
parser(Arg1&& arg1, std::true_type, ArgN&&... argn)
    : m_(std::forward<Arg1>(arg1),
        std::forward<ArgN>(argn)...)
    , rd_(m_)
{
    m_.clear();
#ifndef BOOST_BEAST_ALLOW_DEPRECATED
    /* Deprecated BodyWriter Concept (v1.66) */
    static_assert(sizeof(Body) == 0,
        BOOST_BEAST_DEPRECATION_STRING);
#endif // BOOST_BEAST_ALLOW_DEPRECATED
}

// VFALCO arg1 comes before `false_type` to make
//        the signature unambiguous.
template<bool isRequest, class Body, class Allocator>
template<class Arg1, class... ArgN, class>
parser<isRequest, Body, Allocator>::
parser(Arg1&& arg1, std::false_type, ArgN&&... argn)
    : m_(std::forward<Arg1>(arg1),
        std::forward<ArgN>(argn)...)
    , rd_(m_.base(), m_.body())
{
    m_.clear();
}

template<bool isRequest, class Body, class Allocator>
template<class OtherBody, class... Args, class>
parser<isRequest, Body, Allocator>::
parser(
    parser<isRequest, OtherBody, Allocator>&& other,
    Args&&... args)
    : parser(detail::has_deprecated_body_reader<Body>{},
        std::move(other), std::forward<Args>(args)...)
{
}

template<bool isRequest, class Body, class Allocator>
template<class OtherBody, class... Args, class>
parser<isRequest, Body, Allocator>::
parser(std::true_type,
    parser<isRequest, OtherBody, Allocator>&& other,
    Args&&... args)
    : base_type(std::move(other))
    , m_(other.release(), std::forward<Args>(args)...)
    , rd_(m_)
{
    if(other.rd_inited_)
        BOOST_THROW_EXCEPTION(std::invalid_argument{
            "moved-from parser has a body"});
#ifndef BOOST_BEAST_ALLOW_DEPRECATED
    // Deprecated BodyReader Concept (v1.66)
    static_assert(sizeof(Body) == 0,
        BOOST_BEAST_DEPRECATION_STRING);
#endif
}

template<bool isRequest, class Body, class Allocator>
template<class OtherBody, class... Args, class>
parser<isRequest, Body, Allocator>::
parser(std::false_type, parser<isRequest, OtherBody, Allocator>&& other,
        Args&&... args)
    : base_type(std::move(other))
    , m_(other.release(), std::forward<Args>(args)...)
    , rd_(m_.base(), m_.body())
{
    if(other.rd_inited_)
        BOOST_THROW_EXCEPTION(std::invalid_argument{
            "moved-from parser has a body"});
}

} // http
} // beast
} // boost

#endif
