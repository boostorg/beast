//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/buffer_traits.hpp>

#include <array>

namespace boost {
namespace beast {

BOOST_STATIC_ASSERT(
    std::is_same<net::const_buffer,
    buffers_type<net::const_buffer>>::value);

BOOST_STATIC_ASSERT(
    std::is_same<net::mutable_buffer,
    buffers_type<net::mutable_buffer>>::value);

BOOST_STATIC_ASSERT(
    std::is_same<net::const_buffer,
    buffers_type<std::array<net::const_buffer, 3>>>::value);

BOOST_STATIC_ASSERT(
    std::is_same<net::mutable_buffer,
    buffers_type<std::array<net::mutable_buffer, 3>>>::value);

BOOST_STATIC_ASSERT(
    std::is_same<void,
    buffers_type<std::array<int, 3>>>::value);

} // beast
} // boost
