//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_DETAIL_GET_LOWEST_LAYER_HPP
#define BOOST_BEAST_DETAIL_GET_LOWEST_LAYER_HPP

#include <boost/type_traits/make_void.hpp>
#include <type_traits>

namespace boost {
namespace beast {
namespace detail {

template <class T>
std::false_type has_next_layer_impl(void*) {}

template <class T>
auto has_next_layer_impl(decltype(nullptr)) ->
    decltype(std::declval<T&>().next_layer(), std::true_type{})
{
}

template <class T>
using has_next_layer = decltype(has_next_layer_impl<T>(nullptr));

//---

template<class T, bool = has_next_layer<T>::value>
struct lowest_layer_type_impl
{
    using type = typename std::remove_reference<T>::type;
};

template<class T>
struct lowest_layer_type_impl<T, true>
{
    using type = typename lowest_layer_type_impl<
        decltype(std::declval<T&>().next_layer())>::type;
};

template<class T>
using lowest_layer_type = typename
    lowest_layer_type_impl<T>::type;

//---

template<class T>
T&
get_lowest_layer_impl(
    T& t, std::false_type) noexcept
{
    return t;
}

template<class T>
lowest_layer_type<T>&
get_lowest_layer_impl(
    T& t, std::true_type) noexcept
{
    return get_lowest_layer_impl(t.next_layer(),
        has_next_layer<typename std::decay<
            decltype(t.next_layer())>::type>{});
}

} // detail
} // beast
} // boost

#endif
