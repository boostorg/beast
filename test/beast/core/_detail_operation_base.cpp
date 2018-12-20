//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/detail/operation_base.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/core/ignore_unused.hpp>

//------------------------------------------------------------------------------

namespace boost {
namespace beast {
namespace detail {

namespace {

struct specialized_handler
{
    bool invoked = false;
    struct executor
    {
        void* context() { return nullptr; }
        void on_work_started() {}
        void on_work_finished() {}
        template<class F> void dispatch(F&&) {}
        template<class F> void post(F&&) {}
        template<class F> void defer(F&&) {}
    };
};

template<class Function>
static
void
asio_handler_invoke(
    Function&& f,
    specialized_handler* p)
{
    p->invoked = true;
    f();
}

static
void*
asio_handler_allocate(
    std::size_t,
    specialized_handler* p)
{
    p->invoked = true;
    return nullptr;
}

static
void
asio_handler_deallocate(
    void*, std::size_t,
    specialized_handler* p)
{
    p->invoked = true;
}

static bool
asio_handler_is_continuation(
    specialized_handler* p)
{
    p->invoked = true;
    return false;
}

} // <anonymous>

} // detail
} // beast
} // boost

//------------------------------------------------------------------------------

namespace boost {
namespace asio {

template<class Allocator>
struct associated_allocator<
    boost::beast::detail::specialized_handler, Allocator>
{
    using type = std::allocator<int>;

    static type get(
        boost::beast::detail::specialized_handler const& h,
        Allocator const& a = Allocator()) noexcept
    {
        return type{};
    }
};

template<class Executor>
struct associated_executor<
    boost::beast::detail::specialized_handler, Executor>
{
    using type = typename
        boost::beast::detail::specialized_handler::executor;

    static type get(
        boost::beast::detail::specialized_handler const& h,
        Executor const& ex = Executor()) noexcept
    {
        return type{};
    }
};

} // asio
} // boost

//------------------------------------------------------------------------------

namespace boost {
namespace beast {
namespace detail {

class operation_base_test : public beast::unit_test::suite
{
public:
    using default_alloc = std::allocator<void>;
    using default_exec = net::system_executor;

    struct U {};
    struct V {};

    template<class E>
    struct executor
    {
        void* context() { return nullptr; }
        void on_work_started() {}
        void on_work_finished() {}
        template<class F> void dispatch(F&&) {}
        template<class F> void post(F&&) {}
        template<class F> void defer(F&&) {}
    };

    struct none
    {
        void operator()() const
        {
        }
    };

    struct with_alloc
    {
        using allocator_type = std::allocator<U>;
    };

    struct with_exec
    {
        using executor_type = executor<U>;
    };

    struct move_only
    {
        move_only() = default;
        move_only(move_only&&) = default;
        move_only(move_only const&) = delete;
        void operator()() const{};
    };

    template<
        class H,
        class E = default_exec,
        class A = default_alloc>
    using tested_base =
        operation_base<H, E, A>;

    struct movable_handler : tested_base<move_only>
    {
        movable_handler()
            : tested_base<move_only>(move_only{})
        {
        }
    };

    struct test_handler :
        tested_base<specialized_handler>
    {
        test_handler()
            : tested_base<
                specialized_handler>(
                    specialized_handler{})
        {
        }

        bool invoked() const noexcept
        {
            return this->handler_.invoked;
        }
    };


    // no nested allocator type

    BOOST_STATIC_ASSERT(
        std::is_same<
            default_alloc,
            net::associated_allocator_t<
                tested_base<none>
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            default_alloc,
            decltype(net::get_associated_allocator(
                std::declval<tested_base<none>>()
        ))>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            default_alloc,
            decltype(net::get_associated_allocator(
                std::declval<tested_base<none>>()
        ))>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            std::allocator<U>,
            decltype(net::get_associated_allocator(std::declval<
                tested_base<none, default_exec, std::allocator<U>>>()
        ))>::value);

    // shouldn't work due to net.ts limitations
    BOOST_STATIC_ASSERT(
        ! std::is_same<
            std::allocator<U>,
            decltype(net::get_associated_allocator(
                std::declval<tested_base<none>>(),
                std::declval<std::allocator<U>>()
        ))>::value);
    BOOST_STATIC_ASSERT(
        std::is_same<
            default_alloc,
            decltype(net::get_associated_allocator(
                std::declval<tested_base<none>>(),
                std::declval<std::allocator<U>>()
        ))>::value);

    // nested allocator_type

    BOOST_STATIC_ASSERT(
        std::is_same<
            std::allocator<U>,
            net::associated_allocator_t<
                tested_base<with_alloc>
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            std::allocator<U>,
            decltype(net::get_associated_allocator(
                std::declval<tested_base<with_alloc>>()
        ))>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            std::allocator<U>,
            decltype(net::get_associated_allocator(
                std::declval<tested_base<with_alloc>>(),
                std::declval<std::allocator<V>>()
        ))>::value);

    // specialization of associated_allocator

    BOOST_STATIC_ASSERT(
        std::is_same<
            std::allocator<int>,
            net::associated_allocator_t<
                tested_base<
                    specialized_handler>
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            std::allocator<int>,
            decltype(net::get_associated_allocator(
                std::declval<tested_base<
                    specialized_handler>>()
        ))>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            std::allocator<int>,
            decltype(net::get_associated_allocator(
                std::declval<tested_base<
                    specialized_handler>>(),
                std::declval<std::allocator<V>>()
        ))>::value);

    // no nested executor type

    BOOST_STATIC_ASSERT(
        std::is_same<
            default_exec,
            net::associated_executor_t<
                tested_base<none>
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            executor<U>,
            net::associated_executor_t<
                tested_base<none, executor<U>>
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            default_exec,
            decltype(net::get_associated_executor(
                std::declval<tested_base<none>>()
        ))>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            executor<U>,
            decltype(net::get_associated_executor(std::declval<
                tested_base<none, executor<U>>>()
        ))>::value);

    // shouldn't work due to net.ts limitations
    BOOST_STATIC_ASSERT(
        ! std::is_same<
            executor<U>,
            decltype(net::get_associated_executor(
                std::declval<tested_base<none>>(),
                std::declval<executor<U>>()
        ))>::value);
    BOOST_STATIC_ASSERT(
        std::is_same<
            default_exec,
            decltype(net::get_associated_executor(
                std::declval<tested_base<none>>(),
                std::declval<executor<U>>()
        ))>::value);

    // nested executor_type

    BOOST_STATIC_ASSERT(
        std::is_same<
            executor<U>,
            net::associated_executor_t<
                tested_base<with_exec>
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            executor<U>,
            decltype(net::get_associated_executor(
                std::declval<tested_base<with_exec>>()
        ))>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            executor<U>,
            decltype(net::get_associated_executor(
                std::declval<tested_base<with_exec>>(),
                std::declval<executor<V>>()
        ))>::value);

    // specialization of associated_executor

    BOOST_STATIC_ASSERT(
        std::is_same<
            specialized_handler::executor,
            net::associated_executor_t<
                tested_base<
                    specialized_handler>
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            specialized_handler::executor,
            decltype(net::get_associated_executor(std::declval<
                tested_base<
                    specialized_handler>>()
        ))>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            specialized_handler::executor,
            decltype(net::get_associated_executor(std::declval<
                tested_base<
                    specialized_handler>>(),
                std::declval<executor<V>>()
        ))>::value);

    //--------------------------------------------------------------------------

    template <class Handler>
    struct wrapped_handler : operation_base<
        Handler, net::associated_executor_t<Handler>>
    {
        template <class Handler_>
        explicit wrapped_handler (Handler_&& handler)
            : operation_base<Handler, net::associated_executor_t <Handler>>(
                std::forward<Handler_>(handler), net::get_associated_executor(handler))
        {
        }

        template <class... Args>
        void operator()(Args&&... args)
        {
            this->handler_(std::forward<Args>(args)...);
        }
    };

    void
    testJavadocs()
    {
        wrapped_handler<none>{none{}}();
    }

    //--------------------------------------------------------------------------

    void
    testLegacyHooks()
    {
        // asio_handler_invoke
        {
            test_handler h;
            BEAST_EXPECT(! h.invoked());
            bool invoked = false;
            using net::asio_handler_invoke;
            asio_handler_invoke(
                [&invoked]
                {
                    invoked =true;
                }, &h);
            BEAST_EXPECT(invoked);
            BEAST_EXPECT(h.invoked());
        }

        // asio_handler_allocate
        {
            test_handler h;
            BEAST_EXPECT(! h.invoked());
            using net::asio_handler_allocate;
            asio_handler_allocate(0, &h);
            BEAST_EXPECT(h.invoked());
        }

        // asio_handler_deallocate
        {
            test_handler h;
            BEAST_EXPECT(! h.invoked());
            using net::asio_handler_deallocate;
            asio_handler_deallocate(nullptr, 0, &h);
            BEAST_EXPECT(h.invoked());
        }

        // asio_handler_deallocate
        {
            test_handler h;
            BEAST_EXPECT(! h.invoked());
            using net::asio_handler_is_continuation;
            asio_handler_is_continuation(&h);
            BEAST_EXPECT(h.invoked());
        }
    }

    void
    testSpecialMembers()
    {
        {
            test_handler h1;
            test_handler h2(std::move(h1));
            test_handler h3(h2);
            boost::ignore_unused(h3);
        }

        {
            movable_handler h1;
            movable_handler h2(std::move(h1));
            boost::ignore_unused(h2);
        }
    }

    void
    run() override
    {
        testJavadocs();
        testLegacyHooks();
        testSpecialMembers();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,operation_base);

} // detail
} // beast
} // boost
