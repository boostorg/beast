//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/async_op_base.hpp>

#include "test_handler.hpp"

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/_experimental/test/handler.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/system_executor.hpp>
#include <boost/asio/write.hpp>
#include <boost/core/ignore_unused.hpp>
#include <stdexcept>

//------------------------------------------------------------------------------

namespace boost {
namespace beast {

namespace {

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

} // (anon)

} // beast
} // boost

namespace boost {
namespace asio {

template<class Allocator>
struct associated_allocator<
    boost::beast::handler<
        boost::beast::no_ex,
        boost::beast::intrusive_alloc>,
            Allocator>
{
    using type =
        boost::beast::intrusive_alloc::allocator_type;

    static type get(
        boost::beast::handler<
            boost::beast::no_ex,
            boost::beast::intrusive_alloc> const& h,
        Allocator const& a = Allocator()) noexcept
    {
        return type{};
    }
};

template<class Executor>
struct associated_executor<
    boost::beast::handler<
        boost::beast::intrusive_ex,
        boost::beast::no_alloc>,
            Executor>
{
    using type =
        boost::beast::intrusive_ex::executor_type;

    static type get(
        boost::beast::handler<
            boost::beast::intrusive_ex,
            boost::beast::no_alloc> const& h,
        Executor const& a = Executor()) noexcept
    {
        return type{};
    }
};

} // asio
} // boost

//------------------------------------------------------------------------------

namespace boost {
namespace beast {

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

    struct final_handler
    {
        bool& invoked;

        void
        operator()()
        {
            invoked = true;
        }
    };

    void
    testBase()
    {
        // get_allocator
        {
            simple_allocator alloc;
            simple_allocator alloc2;
            async_op_base<
                move_only_handler,
                simple_executor,
                simple_allocator> op(
                    move_only_handler{}, {}, alloc);
            BEAST_EXPECT(op.get_allocator() == alloc);
            BEAST_EXPECT(op.get_allocator() != alloc2);
        }

        // get_executor
        {
            simple_executor ex;
            simple_executor ex2;
            async_op_base<
                move_only_handler,
                simple_executor> op(
                    move_only_handler{}, ex);
            BEAST_EXPECT(op.get_executor() == ex);
            BEAST_EXPECT(op.get_executor() != ex2);
        }

        // move construction
        {
            async_op_base<
                move_only_handler,
                simple_executor> op(
                    move_only_handler{}, {});
            auto op2 = std::move(op);
        }

        // observers
        {
            bool b = false;
            async_op_base<
                legacy_handler,
                simple_executor> op(
                    legacy_handler{b}, {});
            BEAST_EXPECT(! op.handler().hook_invoked);
            b = true;
            BEAST_EXPECT(op.handler().hook_invoked);
            b = false;
            BEAST_EXPECT(! op.release_handler().hook_invoked);
        }

        // invocation
        {
            net::io_context ioc;
            async_op_base<
                test::handler,
                net::io_context::executor_type> op(
                    test::any_handler(), ioc.get_executor());
            op.invoke(true);
        }
        {
            net::io_context ioc;
            async_op_base<
                test::handler,
                net::io_context::executor_type> op(
                    test::any_handler(), ioc.get_executor());
            op.invoke(false);
            ioc.run();
        }
        {
            async_op_base<
                test::handler,
                simple_executor> op(
                    test::any_handler(), {});
            op.invoke_now();
        }

        // legacy hooks
        legacy_handler::test(
            [](legacy_handler h)
            {
                return async_op_base<
                    legacy_handler,
                    simple_executor>(
                        std::move(h), {});
            });
    }

    void
    testStableBase()
    {
        // get_allocator
        {
            simple_allocator alloc;
            simple_allocator alloc2;
            stable_async_op_base<
                move_only_handler,
                simple_executor,
                simple_allocator> op(
                    move_only_handler{}, {}, alloc);
            BEAST_EXPECT(op.get_allocator() == alloc);
            BEAST_EXPECT(op.get_allocator() != alloc2);
        }

        // get_executor
        {
            simple_executor ex;
            simple_executor ex2;
            stable_async_op_base<
                move_only_handler,
                simple_executor> op(
                    move_only_handler{}, ex);
            BEAST_EXPECT(op.get_executor() == ex);
            BEAST_EXPECT(op.get_executor() != ex2);
        }

        // move construction
        {
            stable_async_op_base<
                move_only_handler,
                simple_executor> op(
                    move_only_handler{}, {});
            auto op2 = std::move(op);
        }

        // invocation
        {
            net::io_context ioc;
            stable_async_op_base<
                test::handler,
                net::io_context::executor_type> op(
                    test::any_handler(), ioc.get_executor());
            op.invoke(true);
        }
        {
            net::io_context ioc;
            stable_async_op_base<
                test::handler,
                net::io_context::executor_type> op(
                    test::any_handler(), ioc.get_executor());
            op.invoke(false);
            ioc.run();
        }
        {
            stable_async_op_base<
                test::handler,
                simple_executor> op(
                    test::any_handler(), {});
            op.invoke_now();
        }

        // legacy hooks
        legacy_handler::test(
            [](legacy_handler h)
            {
                return stable_async_op_base<
                    legacy_handler,
                    simple_executor>(
                        std::move(h), {});
            });

        // allocate_stable

        {
            bool destroyed = false;
            {
                struct data
                {
                    bool& destroyed;

                    ~data()
                    {
                        destroyed = true;
                    }
                };
                stable_async_op_base<
                    move_only_handler,
                    simple_executor> op(
                        move_only_handler{}, {});
                BEAST_EXPECT(! destroyed);
                auto& d = allocate_stable<data>(op, destroyed);
                BEAST_EXPECT(! d.destroyed);
            }
            BEAST_EXPECT(destroyed);
        }
        {
            struct throwing_data
            {
                throwing_data()
                {
                    BOOST_THROW_EXCEPTION(
                        std::exception{});
                }
            };
            stable_async_op_base<
                move_only_handler,
                simple_executor> op(
                    move_only_handler{}, {});
            try
            {
                allocate_stable<throwing_data>(op);
                fail();
            }
            catch(std::exception const&)
            {
                pass();
            }
        }
    }

    //--------------------------------------------------------------------------

    // Asynchronously read into a buffer until the buffer is full, or an error occurs
    template<class AsyncReadStream, class ReadHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(ReadHandler, void (error_code, std::size_t))
    async_read(AsyncReadStream& stream, net::mutable_buffer buffer, ReadHandler&& handler)
    {
        using handler_type = BOOST_ASIO_HANDLER_TYPE(ReadHandler, void(error_code, std::size_t));
        using base_type = async_op_base<handler_type, typename AsyncReadStream::executor_type>;

        struct op : base_type
        {
            AsyncReadStream& stream_;
            net::mutable_buffer buffer_;
            std::size_t total_bytes_transferred_;

            op(
                AsyncReadStream& stream,
                net::mutable_buffer buffer,
                handler_type& handler)
                : base_type(std::move(handler), stream.get_executor())
                , stream_(stream)
                , buffer_(buffer)
                , total_bytes_transferred_(0)
            {
                (*this)({}, 0, false); // start the operation
            }

            void operator()(error_code ec, std::size_t bytes_transferred, bool is_continuation = true)
            {
                // Adjust the count of bytes and advance our buffer
                total_bytes_transferred_ += bytes_transferred;
                buffer_ = buffer_ + bytes_transferred;

                // Keep reading until buffer is full or an error occurs
                if(! ec && buffer_.size() > 0)
                    return stream_.async_read_some(buffer_, std::move(*this));

                // Call the completion handler with the result. If `is_continuation` is
                // false, which happens on the first time through this function, then
                // `net::post` will be used to call the completion handler, otherwise
                // the completion handler will be invoked directly.

                this->invoke(is_continuation, ec, total_bytes_transferred_);
            }
        };

        net::async_completion<ReadHandler, void(error_code, std::size_t)> init{handler};
        op(stream, buffer, init.completion_handler);
        return init.result.get();
    }

    // Asynchronously send a message multiple times, once per second
    template <class AsyncWriteStream, class T, class WriteHandler>
    auto async_write_messages(
        AsyncWriteStream& stream,
        T const& message,
        std::size_t repeat_count,
        WriteHandler&& handler) ->
            typename net::async_result<
                typename std::decay<WriteHandler>::type,
                void(error_code)>::return_type
    {
        using handler_type = typename net::async_completion<WriteHandler, void(error_code)>::completion_handler_type;
        using base_type = stable_async_op_base<handler_type, typename AsyncWriteStream::executor_type>;

        struct op : base_type
        {
            // This object must have a stable address
            struct temporary_data
            {
                std::string const message;
                net::steady_timer timer;

                temporary_data(std::string message_, net::io_context& ctx)
                    : message(std::move(message_))
                    , timer(ctx)
                {
                }
            };

            enum { starting, waiting, writing } state_;
            AsyncWriteStream& stream_;
            std::size_t repeats_;
            temporary_data& data_;

            op(AsyncWriteStream& stream, std::size_t repeats, std::string message, handler_type& handler)
                : base_type(std::move(handler), stream.get_executor())
                , state_(starting)
                , stream_(stream)
                , repeats_(repeats)
                , data_(allocate_stable<temporary_data>(*this, std::move(message), stream.get_executor().context()))
            {
                (*this)(); // start the operation
            }

            void operator()(error_code ec = {}, std::size_t = 0)
            {
                if (!ec)
                {
                    switch (state_)
                    {
                    case starting:
                        // If repeats starts at 0 then we must complete immediately. But we can't call the final
                        // handler from inside the initiating function, so we post our intermediate handler first.
                        if(repeats_ == 0)
                            return net::post(std::move(*this));

                    case writing:
                        if (repeats_ > 0)
                        {
                            --repeats_;
                            state_ = waiting;
                            data_.timer.expires_after(std::chrono::seconds(1));

                            // Composed operation not yet complete.
                            return data_.timer.async_wait(std::move(*this));
                        }

                        // Composed operation complete, continue below.
                        break;

                    case waiting:
                        // Composed operation not yet complete.
                        state_ = writing;
                        return net::async_write(stream_, net::buffer(data_.message), std::move(*this));
                    }
                }

                // The base class destroys the temporary data automatically, before invoking the final completion handler
                this->invoke_now(ec);
            }
        };

        net::async_completion<WriteHandler, void(error_code)> completion(handler);
        std::ostringstream os;
        os << message;
        op(stream, repeat_count, os.str(), completion.completion_handler);
        return completion.result.get();
    }

    void
    testJavadocs()
    {
        struct handler
        {
            void operator()(error_code = {}, std::size_t = 0)
            {
            }
        };

        BEAST_EXPECT((&async_op_base_test::async_read<test::stream, handler>));
        BEAST_EXPECT((&async_op_base_test::async_write_messages<test::stream, std::string, handler>));
    }

    //--------------------------------------------------------------------------

    void
    run() override
    {
        testBase();
        testStableBase();
        testJavadocs();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,async_op_base);

} // beast
} // boost
