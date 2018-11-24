//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_TIMEOUT_WORK_GUARD_HPP
#define BOOST_BEAST_CORE_TIMEOUT_WORK_GUARD_HPP

#include <boost/beast/_experimental/core/timeout_service.hpp>
#include <boost/assert.hpp>

namespace boost {
namespace beast {

class timeout_work_guard
{
    timeout_handle h_;

public:
    timeout_work_guard(timeout_work_guard const&) = delete;
    timeout_work_guard& operator=(timeout_work_guard const&) = delete;

    ~timeout_work_guard()
    {
        reset();
    }

    timeout_work_guard(timeout_work_guard&& other)
        : h_(other.h_)
    {
        other.h_ = nullptr;
    }

    explicit
    timeout_work_guard(timeout_handle h)
        : h_(h)
    {
        h_.service().on_work_started(h_);
    }

    bool
    owns_work() const
    {
        return h_ != nullptr;
    }

    void
    reset()
    {
        if(h_)
            h_.service().on_work_stopped(h_);
    }

    bool
    try_complete()
    {
        BOOST_ASSERT(h_ != nullptr);
        auto result =
            h_.service().on_try_work_complete(h_);
        h_ = nullptr;
        return result;
    }
};

} // beast
} // boost

#endif
