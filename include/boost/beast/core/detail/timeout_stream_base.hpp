//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DETAIL_TIMEOUT_STREAM_BASE_HPP
#define BOOST_BEAST_CORE_DETAIL_TIMEOUT_STREAM_BASE_HPP

#include <boost/assert.hpp>
#include <boost/core/exchange.hpp>

namespace boost {
namespace beast {
namespace detail {

template<class, class, class>
class timeout_stream_connect_op;

struct any_endpoint
{
    template<class Error, class Endpoint>
    bool
    operator()(
        Error const&, Endpoint const&) const noexcept
    {
        return true;
    }
};

class timeout_stream_base
{
protected:
    class pending_guard
    {
        bool& b_;
        bool clear_ = true;

    public:
        ~pending_guard()
        {
            if(clear_)
                b_ = false;
        }

        explicit
        pending_guard(bool& b)
            : b_(b)
        {
            BOOST_ASSERT(! b_);
            b_ = true;
        }

        pending_guard(
            pending_guard&& other) noexcept
            : b_(other.b_)
            , clear_(boost::exchange(
                other.clear_, false))
        {
        }

        void
        reset()
        {
            BOOST_ASSERT(clear_);
            b_ = false;
            clear_ = false;
        }
    };
};

} // detail
} // beast
} // boost

#endif
