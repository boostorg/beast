//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_DETAIL_SAVED_HANDLER_HPP
#define BOOST_BEAST_WEBSOCKET_DETAIL_SAVED_HANDLER_HPP

#include <boost/beast/core/detail/allocator.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/assert.hpp>
#include <memory>
#include <utility>

namespace boost {
namespace beast {
namespace websocket {
namespace detail {

// A container that holds a suspended, asynchronous composed
// operation. The contained object may be invoked later to
// resume the operation, or the container may be destroyed.
//
class saved_handler
{
    struct base
    {
        base() = default;
        base(base &&) = delete;
        base(base const&) = delete;
        virtual void destroy() = 0;
        virtual void invoke() = 0;

    protected:
        ~base() = default;
    };

    template<class Handler>
    class impl : public base
    {
        Handler h_;
        net::executor_work_guard<
            net::associated_executor_t<Handler>> wg_;

    public:
        template<class DeducedHandler>
        impl(DeducedHandler&& h)
            : h_(std::forward<DeducedHandler>(h))
            , wg_(net::get_associated_executor(h_))
        {
        }

        void
        destroy() override
        {
            using A = typename beast::detail::allocator_traits<
                net::associated_allocator_t<
                    Handler>>::template rebind_alloc<impl>;
            using alloc_traits =
                beast::detail::allocator_traits<A>;
            Handler h(std::move(h_));
            A alloc(net::get_associated_allocator(h));
            alloc_traits::destroy(alloc, this);
            alloc_traits::deallocate(alloc, this, 1);
        }

        void
        invoke() override
        {
            using A = typename beast::detail::allocator_traits<
                net::associated_allocator_t<
                    Handler>>::template rebind_alloc<impl>;
            using alloc_traits =
                beast::detail::allocator_traits<A>;
            Handler h(std::move(h_));
            A alloc(net::get_associated_allocator(h));
            alloc_traits::destroy(alloc, this);
            alloc_traits::deallocate(alloc, this, 1);
            h();
        }
    };

    base* p_ = nullptr;

public:
    saved_handler() = default;
    saved_handler(saved_handler const&) = delete;
    saved_handler& operator=(saved_handler const&) = delete;

    ~saved_handler()
    {
        if(p_)
            p_->destroy();
    }

    saved_handler(saved_handler&& other)
    {
        boost::ignore_unused(other);
        // Moving a saved handler that
        // owns an object is illegal
        BOOST_ASSERT(! other.p_);
    }

    saved_handler&
    operator=(saved_handler&& other)
    {
        boost::ignore_unused(other);
        // Moving a saved handler that
        // owns an object is illegal
        BOOST_ASSERT(! p_);
        BOOST_ASSERT(! other.p_);
        return *this;
    }

    template<class Handler>
    void
    emplace(Handler&& handler)
    {
        // Can't emplace twice without invoke
        BOOST_ASSERT(! p_);
        using A = typename beast::detail::allocator_traits<
            net::associated_allocator_t<Handler>>::template
                rebind_alloc<impl<Handler>>;
        using alloc_traits =
            beast::detail::allocator_traits<A>;
        A alloc(net::get_associated_allocator(handler));
        auto const d =
            [&alloc](impl<Handler>* p)
            {
                alloc_traits::deallocate(alloc, p, 1);
            };
        std::unique_ptr<impl<Handler>, decltype(d)> p(
            alloc_traits::allocate(alloc, 1), d);
        alloc_traits::construct(alloc, p.get(),
            std::forward<Handler>(handler));
        p_ = p.release();
    }

    explicit
    operator bool() const noexcept
    {
        return p_ != nullptr;
    }

    bool
    maybe_invoke()
    {
        if(p_)
        {
            auto const h = p_;
            p_ = nullptr;
            h->invoke();
            return true;
        }
        return false;
    }
};

} // detail
} // websocket
} // beast
} // boost

#endif
