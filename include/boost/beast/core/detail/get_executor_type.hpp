//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_DETAIL_GET_EXECUTOR_TYPE
#define BOOST_BEAST_DETAIL_GET_EXECUTOR_TYPE

#include <boost/config/workaround.hpp>
#include <utility>

namespace boost {
namespace beast {
namespace detail {

// Workaround for ICE on gcc 4.8
#if BOOST_WORKAROUND(BOOST_GCC, < 40900)
template<class T>
using get_executor_type =
    typename std::decay<T>::type::executor_type;
#else
template<class T>
using get_executor_type =
    decltype(std::declval<T&>().get_executor());
#endif

} // detail
} // beast
} // boost

#endif
