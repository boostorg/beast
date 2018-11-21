//
// Copyright (c) 2018 Damian Jarek (damian dot jarek93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//


#ifndef BOOST_BEAST_DETAIL_LEAN_TUPLE_HPP
#define BOOST_BEAST_DETAIL_LEAN_TUPLE_HPP

#include <boost/mp11/integer_sequence.hpp>
#include <boost/mp11/algorithm.hpp>

#include <boost/type_traits/remove_cv.hpp>
#include <boost/type_traits/copy_cv.hpp>

namespace boost
{
namespace beast
{
namespace detail
{

template<std::size_t I, typename T>
struct lean_tuple_element
{
    T t_;
};

template<typename... Ts>
struct lean_tuple_impl;

template<typename... Ts, std::size_t... Is>
struct lean_tuple_impl<boost::mp11::index_sequence<Is...>, Ts...>
  : lean_tuple_element<Is, Ts>...
{
    template<typename... Us>
    explicit lean_tuple_impl(Us&&... us)
      : lean_tuple_element<Is, Ts>{std::forward<Us>(us)}...
    {
    }
};

template<typename... Ts>
struct lean_tuple : lean_tuple_impl<boost::mp11::index_sequence_for<Ts...>, Ts...>
{
    template<typename... Us>
    explicit lean_tuple(Us&&... us)
      : lean_tuple_impl<boost::mp11::index_sequence_for<Ts...>, Ts...>{
          std::forward<Us>(us)...}
    {
    }
};

template<std::size_t I, typename T>
T&
get(lean_tuple_element<I, T>& te)
{
    return te.t_;
}

template<std::size_t I, typename T>
T const&
get(lean_tuple_element<I, T> const& te)
{
    return te.t_;
}

template<std::size_t I, typename T>
T&&
get(lean_tuple_element<I, T>&& te)
{
    return std::move(te.t_);
}

template <std::size_t I, typename T>
using tuple_element_t = typename copy_cv<mp11::mp_at_c<remove_cv_t<T>, I>, T>::type;

} // namespace detail
} // namespace beast
} // namespace boost

#endif // BOOST_BEAST_DETAIL_LEAN_TUPLE_HPP
