//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DETAIL_SAVED_HANDLER_HPP
#define BOOST_BEAST_CORE_DETAIL_SAVED_HANDLER_HPP

#include <boost/beast/core/bind_handler.hpp>
#include <memory>

namespace boost {
namespace beast {
namespace detail {

class saved_handler
{
    struct base
    {
        virtual ~base() = default;
        virtual void operator()() = 0;
    };

    template<class Handler>
    struct impl : base
    {
        Handler h_;

        template<class Deduced>
        explicit
        impl(Deduced&& h)
            : h_(std::forward<Deduced>(h))
        {
        }

        void operator()() override
        {
            h_();
        }
    };

    std::unique_ptr<base> p_;

public:
    saved_handler() = default;

    template<class Handler>
    void
    emplace(Handler&& h)
    {
        p_.reset(new impl<typename
            std::decay<Handler>::type>(
                std::forward<Handler>(h)));
    }

    template<class Handler,
        class T0, class... TN>
    void
    emplace(Handler&& h,
        T0&& t0, TN&... tn)
    {
        using type = decltype(
            beast::bind_front_handler(
                std::forward<Handler>(h),
                std::forward<T0>(t0),
                std::forward<TN>(tn)...));
        p_.reset(new impl<type>(
            beast::bind_front_handler(
                std::forward<Handler>(h),
                std::forward<T0>(t0),
                std::forward<TN>(tn)...)));
    }

    bool
    empty() const noexcept
    {
        return p_.get() == nullptr;
    }

    explicit
    operator bool() const noexcept
    {
        return ! empty();
    }

    void
    operator()()
    {
        auto p = std::move(p_);
        (*p)();
    }

    void
    reset()
    {
        p_.reset(nullptr);
    }
};


} // detail
} // beast
} // boost

#endif
