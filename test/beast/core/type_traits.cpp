//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/type_traits.hpp>

#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/detail/consuming_buffers.hpp>
#include <memory>

namespace boost {
namespace beast {

//
// handler concepts
//

namespace {

struct H
{
    void operator()(int){}
};

} // anonymous

BOOST_STATIC_ASSERT(is_completion_handler<H, void(int)>::value);
BOOST_STATIC_ASSERT(! is_completion_handler<H, void(void)>::value);

} // beast
} // boost
