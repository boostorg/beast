//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DETAIL_ASYNC_OP_BASE_HPP
#define BOOST_BEAST_CORE_DETAIL_ASYNC_OP_BASE_HPP

#include <boost/core/exchange.hpp>

namespace boost {
namespace beast {
namespace detail {

struct stable_base
{
    static
    void
    destroy_list(stable_base*& list)
    {
        while(list)
        {
            auto next = list->next_;
            list->destroy();
            list = next;
        }
    }

protected:
    stable_base* next_;
    virtual void destroy() = 0;
    virtual ~stable_base() = default;
    explicit stable_base(stable_base*& list)
        : next_(boost::exchange(list, this))
    {
    }
};

} // detail
} // beast
} // boost

#endif
