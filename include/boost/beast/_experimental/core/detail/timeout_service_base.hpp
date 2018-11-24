//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DETAIL_TIMEOUT_SERVICE_BASE_HPP
#define BOOST_BEAST_CORE_DETAIL_TIMEOUT_SERVICE_BASE_HPP

#include <boost/beast/_experimental/core/detail/saved_handler.hpp>

#include <vector>

namespace boost {
namespace beast {
namespace detail {

struct thunk
{
    using list_type =
        std::vector<thunk*>;

    saved_handler callback;
    list_type* list = nullptr;
    std::size_t pos = 0; // also: next in free list
    bool expired = false;
    bool canceled = false;
    bool completed = false;
};

} // detail
} // beast
} // boost

#endif
