//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DETAIL_ASYNC_BASE_HPP
#define BOOST_BEAST_CORE_DETAIL_ASYNC_BASE_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/type_traits.hpp>

#include <boost/asio/associated_immediate_executor.hpp>
#include <boost/asio/cancellation_type.hpp>

namespace boost {
namespace beast {
namespace detail {

struct allowed_cancellation_filter
{
    net::cancellation_type mask;

    net::cancellation_type
    operator()(net::cancellation_type type) const noexcept
    {
        return type & mask;
    }
};

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

    stable_base* next_ = nullptr;

protected:
    stable_base() = default;
    virtual ~stable_base() = default;

    virtual void destroy() = 0;
};

template<typename Handler, typename = void>
struct with_immediate_executor_type
{
};

template<typename Handler>
struct with_immediate_executor_type<Handler,
    void_t<typename Handler::immediate_executor_type>>
{
    using immediate_executor_type = typename Handler::immediate_executor_type;
};

} // detail
} // beast
} // boost

#endif
