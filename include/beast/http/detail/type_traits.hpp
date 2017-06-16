//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_DETAIL_TYPE_TRAITS_HPP
#define BEAST_HTTP_DETAIL_TYPE_TRAITS_HPP

#include <beast/core/detail/type_traits.hpp>

namespace beast {
namespace http {

template<bool isRequest, class Fields>
struct header;

template<bool, class, class>
struct message;

template<bool isRequest,class Body, class Fields>
class parser;

namespace detail {

template<class T>
class is_header_impl
{
    template<bool b, class F>
    static std::true_type check(
        header<b, F> const*);
    static std::false_type check(...);
public:
    using type = decltype(check((T*)0));
};

template<class T>
using is_header = typename is_header_impl<T>::type;

template<class T>
struct is_parser : std::false_type {};

template<bool isRequest, class Body, class Fields>
struct is_parser<parser<isRequest, Body, Fields>> : std::true_type {};

struct fields_model
{
    string_view method() const;
    string_view reason() const;
    string_view target() const;
};

template<class T, class = beast::detail::void_t<>>
struct has_value_type : std::false_type {};

template<class T>
struct has_value_type<T, beast::detail::void_t<
    typename T::value_type
        > > : std::true_type {};

/** Determine if a @b Body type has a size

    This metafunction is equivalent to `std::true_type` if
    Body contains a static member function called `size`.
*/
template<class T, class = void>
struct is_body_sized : std::false_type {};

template<class T>
struct is_body_sized<T, beast::detail::void_t<
    typename T::value_type,
        decltype(
    std::declval<std::uint64_t&>() =
        T::size(std::declval<typename T::value_type const&>()),
    (void)0)>> : std::true_type {};

} // detail
} // http
} // beast

#endif
