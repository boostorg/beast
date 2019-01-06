//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/detail/async_op_base.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/system_executor.hpp>
#include <boost/core/ignore_unused.hpp>

//------------------------------------------------------------------------------

namespace boost {
namespace beast {
namespace detail {

//namespace {

struct ex1_type
{
    void* context() { return nullptr; }
    void on_work_started() {}
    void on_work_finished() {}
    template<class F> void dispatch(F&&) {}
    template<class F> void post(F&&) {}
    template<class F> void defer(F&&) {}
};

struct no_alloc
{
};

struct nested_alloc
{
    struct allocator_type
    {
    };
};

struct intrusive_alloc
{
    struct allocator_type
    {
    };
};

struct no_ex
{
    using executor_type = net::system_executor;
};

struct nested_ex
{
    struct executor_type
    {
    };
};

struct intrusive_ex
{
    struct executor_type
    {
    };
};

template<class E, class A>
struct handler;

template<>
struct handler<no_ex, no_alloc>
{
};

template<>
struct handler<no_ex, nested_alloc>
    : nested_alloc
{
};

template<>
struct handler<no_ex, intrusive_alloc>
{
};

template<>
struct handler<nested_ex, no_alloc>
    : nested_ex
{
};

template<>
struct handler<intrusive_ex, no_alloc>
{
};

struct legacy_handler
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

    executor
    get_executor() const noexcept
    {
        return {};
    };
};

template<class Function>
void
asio_handler_invoke(
    Function&& f,
    legacy_handler* p)
{
    p->invoked = true;
    f();
}

void*
asio_handler_allocate(
    std::size_t,
    legacy_handler* p)
{
    p->invoked = true;
    return nullptr;
}

void
asio_handler_deallocate(
    void*, std::size_t,
    legacy_handler* p)
{
    p->invoked = true;
}

bool
asio_handler_is_continuation(
    legacy_handler* p)
{
    p->invoked = true;
    return false;
}

//} // (anon)

} // detail
} // beast
} // boost

namespace boost {
namespace asio {

template<class Allocator>
struct associated_allocator<
    boost::beast::detail::handler<
        boost::beast::detail::no_ex,
        boost::beast::detail::intrusive_alloc>,
            Allocator>
{
    using type =
        boost::beast::detail::intrusive_alloc::allocator_type;

    static type get(
        boost::beast::detail::handler<
            boost::beast::detail::no_ex,
            boost::beast::detail::intrusive_alloc> const& h,
        Allocator const& a = Allocator()) noexcept
    {
        return type{};
    }
};

template<class Executor>
struct associated_executor<
    boost::beast::detail::handler<
        boost::beast::detail::intrusive_ex,
        boost::beast::detail::no_alloc>,
            Executor>
{
    using type =
        boost::beast::detail::intrusive_ex::executor_type;

    static type get(
        boost::beast::detail::handler<
            boost::beast::detail::intrusive_ex,
            boost::beast::detail::no_alloc> const& h,
        Executor const& a = Executor()) noexcept
    {
        return type{};
    }
};

template<class Allocator>
struct associated_allocator<
    boost::beast::detail::legacy_handler, Allocator>
{
    using type = std::allocator<int>;

    static type get(
        boost::beast::detail::legacy_handler const& h,
        Allocator const& a = Allocator()) noexcept
    {
        return type{};
    }
};

template<class Executor>
struct associated_executor<
    boost::beast::detail::legacy_handler, Executor>
{
    using type = typename
        boost::beast::detail::legacy_handler::executor;

    static type get(
        boost::beast::detail::legacy_handler const&,
        Executor const& = Executor()) noexcept
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

class async_op_base_test : public beast::unit_test::suite
{
public:
    // no associated allocator

    BOOST_STATIC_ASSERT(
        std::is_same<
            std::allocator<void>,
            net::associated_allocator_t<
                async_op_base<
                    handler<no_ex, no_alloc>,
                    net::io_context::executor_type>
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            std::allocator<int>,
            net::associated_allocator_t<
                async_op_base<
                    handler<no_ex, no_alloc>,
                    net::io_context::executor_type,
                    std::allocator<int>>
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            std::allocator<void>,
            net::associated_allocator_t<
                async_op_base<
                    handler<no_ex, no_alloc>,
                    net::io_context::executor_type>,
                std::allocator<int> // ignored
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            std::allocator<int>,
            net::associated_allocator_t<
                async_op_base<
                    handler<no_ex, no_alloc>,
                    net::io_context::executor_type,
                    std::allocator<int>>,
                std::allocator<double> // ignored
        >>::value);

    // nested associated allocator

    BOOST_STATIC_ASSERT(
        std::is_same<
            nested_alloc::allocator_type,
            net::associated_allocator_t<
                async_op_base<
                    handler<no_ex, nested_alloc>,
                    net::io_context::executor_type>
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            nested_alloc::allocator_type,
            net::associated_allocator_t<
                async_op_base<
                    handler<no_ex, nested_alloc>,
                    net::io_context::executor_type,
                    std::allocator<int>> // ignored
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            nested_alloc::allocator_type,
            net::associated_allocator_t<
                async_op_base<
                    handler<no_ex, nested_alloc>,
                    net::io_context::executor_type>,
                std::allocator<int> // ignored
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            nested_alloc::allocator_type,
            net::associated_allocator_t<
                async_op_base<
                    handler<no_ex, nested_alloc>,
                    net::io_context::executor_type,
                    std::allocator<int>>, // ignored
                std::allocator<int> // ignored
        >>::value);

    // intrusive associated allocator

    BOOST_STATIC_ASSERT(
        std::is_same<
            intrusive_alloc::allocator_type,
            net::associated_allocator_t<
                async_op_base<
                    handler<no_ex, intrusive_alloc>,
                    net::io_context::executor_type>
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            intrusive_alloc::allocator_type,
            net::associated_allocator_t<
                async_op_base<
                    handler<no_ex, intrusive_alloc>,
                    net::io_context::executor_type,
                    std::allocator<int>> // ignored
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            intrusive_alloc::allocator_type,
            net::associated_allocator_t<
                async_op_base<
                    handler<no_ex, intrusive_alloc>,
                    net::io_context::executor_type>,
                std::allocator<int> // ignored
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            intrusive_alloc::allocator_type,
            net::associated_allocator_t<
                async_op_base<
                    handler<no_ex, intrusive_alloc>,
                    net::io_context::executor_type,
                    std::allocator<int>>, // ignored
                std::allocator<int> // ignored
        >>::value);

    // no associated executor

    BOOST_STATIC_ASSERT(
        std::is_same<
            ex1_type,
            net::associated_executor_t<
                async_op_base<
                    handler<no_ex, no_alloc>,
                    ex1_type>
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            ex1_type,
            net::associated_executor_t<
                async_op_base<
                    handler<no_ex, no_alloc>,
                    ex1_type>,
                net::system_executor // ignored
        >>::value);

    // nested associated executor

    BOOST_STATIC_ASSERT(
        std::is_same<
            nested_ex::executor_type,
            net::associated_executor_t<
                async_op_base<
                    handler<nested_ex, no_alloc>,
                    ex1_type>
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            nested_ex::executor_type,
            net::associated_executor_t<
                async_op_base<
                    handler<nested_ex, no_alloc>,
                    ex1_type>,
                net::system_executor // ignored
        >>::value);

    // intrusive associated executor

    BOOST_STATIC_ASSERT(
        std::is_same<
            intrusive_ex::executor_type,
            net::associated_executor_t<
                async_op_base<
                    handler<intrusive_ex, no_alloc>,
                    ex1_type>
        >>::value);

    BOOST_STATIC_ASSERT(
        std::is_same<
            intrusive_ex::executor_type,
            net::associated_executor_t<
                async_op_base<
                    handler<intrusive_ex, no_alloc>,
                    ex1_type>,
                net::system_executor // ignored
        >>::value);

    struct test_op : async_op_base<
        legacy_handler, ex1_type>
    {
        test_op()
            : async_op_base<
                legacy_handler,
                ex1_type>(
                    ex1_type{}, legacy_handler{})
        {

        }

        bool invoked() const noexcept
        {
            return this->handler().invoked;
        }
    };

    void
    testLegacyHooks()
    {
        // asio_handler_invoke
        {
            test_op h;
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
            test_op h;
            BEAST_EXPECT(! h.invoked());
            using net::asio_handler_allocate;
            asio_handler_allocate(0, &h);
            BEAST_EXPECT(h.invoked());
        }

        // asio_handler_deallocate
        {
            test_op h;
            BEAST_EXPECT(! h.invoked());
            using net::asio_handler_deallocate;
            asio_handler_deallocate(nullptr, 0, &h);
            BEAST_EXPECT(h.invoked());
        }

        // asio_handler_deallocate
        {
            test_op h;
            BEAST_EXPECT(! h.invoked());
            using net::asio_handler_is_continuation;
            asio_handler_is_continuation(&h);
            BEAST_EXPECT(h.invoked());
        }
    }

    void
    testSpecialMembers()
    {
        test_op h1;
        test_op h2(std::move(h1));
    }

    //--------------------------------------------------------------------------

#if 0
    // VFALCO TODO: This will become the example in the javadoc

    template<
        class AsyncReadStream,
        class MutableBufferSequence,
        class ReadHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(
        ReadHandler, void (error_code, std::size_t))
    async_read(
        AsyncReadStream& stream,
        MutableBufferSequence const& buffers,
        ReadHandler&& handler)
    {
        net::async_completion<CompletionToken, void(error_code, std::size_t)> init{handler};

        using base = async_op_base<
            BOOST_ASIO_HANDLER_TYPE(ReadHandler, void(error_code, std::size_t)),
            get_executor_type<AsyncReadStream>>;

        struct read_op : base
        {
            AsyncReadStream& stream_;
            buffers_suffix<MutableBufferSequence> buffers_;

            void operator()(error_code ec, std::size_t bytes_transferred)
            {

            }
        };

        read_op(stream, buffers, std::forward<ReadHandler>(handler));
        return init.result.get();
    }
#endif

    void
    testJavadocs()
    {
    }

    //--------------------------------------------------------------------------

    void
    run() override
    {
        testLegacyHooks();
        testSpecialMembers();
        testJavadocs();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,async_op_base);

} // detail
} // beast
} // boost
