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

template<class T, class = void>
struct has_next_layer : std::false_type
{
};

template<class T>
struct has_next_layer<T, boost::void_t<
    decltype(std::declval<T>().next_layer())>>
    : std::true_type
{
};

template<class T, class = void>
struct lowest_layer_type_impl
{
    using type = typename std::remove_reference<T>::type;
};

template<class T>
struct lowest_layer_type_impl<T, boost::void_t<
    decltype(std::declval<T>().next_layer())>>
{
    using type = typename lowest_layer_type_impl<
        decltype(std::declval<T>().next_layer())>::type;
};

template<class T>
using lowest_layer_type = typename
    lowest_layer_type_impl<T>::type;

template<class T>
lowest_layer_type<T>&
get_lowest_layer_impl(
    T& t, std::true_type) noexcept
{
    using type = typename std::decay<
        decltype(t.next_layer())>::type;
    return get_lowest_layer_impl(t.next_layer(),
        has_next_layer<type>{});
}

template<class T>
T&
get_lowest_layer_impl(
    T& t, std::false_type) noexcept
{
    return t;
}

} // detail
} // beast
} // boost

#endif
