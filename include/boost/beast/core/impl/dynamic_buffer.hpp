//
// Copyright (c) 2020 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_IMPL_DYNAMIC_BUFFER_HPP
#define BOOST_BEAST_CORE_IMPL_DYNAMIC_BUFFER_HPP

#include <boost/beast/core/detail/dynamic_buffer_v0.hpp>
#include <memory>

namespace boost {
namespace beast {


template<class DynamicBuffer_v0>
auto
dynamic_buffer(DynamicBuffer_v0& target) ->
#if BOOST_BEAST_DOXYGEN
__see_below__;
#else
typename std::enable_if<
    detail::is_dynamic_buffer_v0<DynamicBuffer_v0>::value,
    detail::dynamic_buffer_v0_proxy<DynamicBuffer_v0>>::type
#endif
{
    return detail::impl_dynamic_buffer(target);
}

} // beast
} // boost

#endif
