//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/bind_handler.hpp>

#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/defer.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/bind/placeholders.hpp>
#include <boost/core/exchange.hpp>
#include <memory>
#include <string>

namespace boost {
namespace beast {

//------------------------------------------------------------------------------

class bind_handler_test : public unit_test::suite
{
public:
    template<class... Args>
    struct handler
    {
        void
        operator()(Args const&...) const
        {
        }
    };

    struct copyable
    {
        template<class... Args>
        void
        operator()(Args&&...) const
        {
        }
    };

    struct move_only
    {
        move_only() = default;
        move_only(move_only&&) = default;
        move_only(move_only const&) = delete;
        void operator()() const{};
    };

    // A move-only parameter
    template<std::size_t I>
    struct move_arg
    {
        move_arg() = default;
        move_arg(move_arg&&) = default;
        move_arg(move_arg const&) = delete;
        void operator()() const
        {
        };
    };

    void
    callback(int v)
    {
        BEAST_EXPECT(v == 42);
    }

    class test_executor
    {
        bind_handler_test& s_;
        net::io_context::executor_type ex_;

    public:
        test_executor(
            test_executor const&) = default;

        test_executor(
            bind_handler_test& s,
            net::io_context& ioc)
            : s_(s)
            , ex_(ioc.get_executor())
        {
        }

        bool operator==(
            test_executor const& other) const noexcept
        {
            return ex_ == other.ex_;
        }

        bool operator!=(
            test_executor const& other) const noexcept
        {
            return ex_ != other.ex_;
        }

        net::io_context&
        context() const noexcept
        {
            return ex_.context();
        }

        void on_work_started() const noexcept
        {
            ex_.on_work_started();
        }

        void on_work_finished() const noexcept
        {
            ex_.on_work_finished();
        }

        template<class F, class Alloc>
        void dispatch(F&& f, Alloc const& a)
        {
            s_.on_invoke();
            ex_.dispatch(std::forward<F>(f), a);
        }

        template<class F, class Alloc>
        void post(F&& f, Alloc const& a)
        {
            // shouldn't be called since the enclosing
            // networking wrapper only uses dispatch
            s_.fail("unexpected post", __FILE__, __LINE__);
        }

        template<class F, class Alloc>
        void defer(F&& f, Alloc const& a)
        {
            // shouldn't be called since the enclosing
            // networking wrapper only uses dispatch
            s_.fail("unexpected defer", __FILE__, __LINE__);
        }
    };

    class test_cb
    {
        bind_handler_test& s_;
        bool fail_ = true;
    
    public:
        test_cb(test_cb const&) = delete;

        test_cb(bind_handler_test& s)
            : s_(s)
        {
        }

        test_cb(test_cb&& other)
            : s_(other.s_)
            , fail_(boost::exchange(
                other.fail_, false))
        {
        }

        ~test_cb()
        {
            if(fail_)
                s_.fail("handler not invoked",
                    __FILE__, __LINE__);
        }

        void
        operator()()
        {
            fail_ = false;
            s_.pass();
        }

        void
        operator()(int v)
        {
            fail_ = false;
            s_.expect(
                v == 42, __FILE__, __LINE__);
        }

        void
        operator()(int v, string_view s)
        {
            fail_ = false;
            s_.expect(
                v == 42, __FILE__, __LINE__);
            s_.expect(
                s == "s", __FILE__, __LINE__);
        }

        void
        operator()(int v, string_view s, move_arg<1>)
        {
            fail_ = false;
            s_.expect(
                v == 42, __FILE__, __LINE__);
            s_.expect(
                s == "s", __FILE__, __LINE__);
        }

        void
        operator()(int v, string_view s, move_arg<1>, move_arg<2>)
        {
            fail_ = false;
            s_.expect(
                v == 42, __FILE__, __LINE__);
            s_.expect(
                s == "s", __FILE__, __LINE__);
        }

        void
        operator()(error_code, std::size_t n)
        {
            fail_ = false;
            s_.expect(
                n == 256, __FILE__, __LINE__);
        }

        void
        operator()(
            error_code, std::size_t n, string_view s)
        {
            boost::ignore_unused(s);
            fail_ = false;
            s_.expect(
                n == 256, __FILE__, __LINE__);
        }

        void
        operator()(std::shared_ptr<int> const& sp)
        {
            fail_ = false;
            s_.expect(sp.get() != nullptr,
                __FILE__, __LINE__);
        }
    };

#if 0
    // These functions should fail to compile

    void
    failStdBind()
    {
        std::bind(bind_handler(test_cb{*this}));
    }

    void
    failStdBindFront()
    {
        std::bind(bind_front_handler(test_cb{*this}));
    }
#endif

    //--------------------------------------------------------------------------

    bool invoked_;

    void
    on_invoke()
    {
        invoked_ = true;
    }

    template<class F>
    void
    testHooks(net::io_context& ioc, F&& f)
    {
        invoked_ = false;
        net::post(ioc, std::forward<F>(f));
        ioc.run();
        ioc.restart();
        BEAST_EXPECT(invoked_);
    }

    //--------------------------------------------------------------------------

    void
    testBindHandler()
    {
        using m1 = move_arg<1>;
        using m2 = move_arg<2>;

        {
            using namespace std::placeholders;

            // 0-ary
            bind_handler(test_cb{*this})();

            // 1-ary
            bind_handler(test_cb{*this}, 42)(); 
            bind_handler(test_cb{*this}, _1)(42);
            bind_handler(test_cb{*this}, _2)(0, 42);

            // 2-ary
            bind_handler(test_cb{*this}, 42, "s")();
            bind_handler(test_cb{*this}, 42, "s")(0);
            bind_handler(test_cb{*this}, _1, "s")(42);
            bind_handler(test_cb{*this}, 42, _1) ("s");
            bind_handler(test_cb{*this}, _1, _2)(42, "s");
            bind_handler(test_cb{*this}, _1, _2)(42, "s", "X");
            bind_handler(test_cb{*this}, _2, _1)("s", 42);
            bind_handler(test_cb{*this}, _3, _2)("X", "s", 42);

            // 3-ary
            bind_handler(test_cb{*this}, 42, "s")(m1{});
            bind_handler(test_cb{*this}, 42, "s", _1)(m1{});
            bind_handler(test_cb{*this}, 42, _1, m1{})("s");

            // 4-ary
            bind_handler(test_cb{*this}, 42, "s")(m1{}, m2{});
            bind_handler(test_cb{*this}, 42, "s", m1{})(m2{});
            bind_handler(test_cb{*this}, 42, "s", m1{}, m2{})();
            bind_handler(test_cb{*this}, 42, _1, m1{})("s", m2{});
            bind_handler(test_cb{*this}, _3, _1, m1{})("s", m2{}, 42);
        }

        {
            using namespace boost::placeholders;

            // 0-ary
            bind_handler(test_cb{*this})();

            // 1-ary
            bind_handler(test_cb{*this}, 42)(); 
            bind_handler(test_cb{*this}, _1)(42);
            bind_handler(test_cb{*this}, _2)(0, 42);

            // 2-ary
            bind_handler(test_cb{*this}, 42, "s")();
            bind_handler(test_cb{*this}, 42, "s")(0);
            bind_handler(test_cb{*this}, _1, "s")(42);
            bind_handler(test_cb{*this}, 42, _1) ("s");
            bind_handler(test_cb{*this}, _1, _2)(42, "s");
            bind_handler(test_cb{*this}, _1, _2)(42, "s", "X");
            bind_handler(test_cb{*this}, _2, _1)("s", 42);
            bind_handler(test_cb{*this}, _3, _2)("X", "s", 42);

            // 3-ary
            bind_handler(test_cb{*this}, 42, "s")(m1{});
            bind_handler(test_cb{*this}, 42, "s", _1)(m1{});
            bind_handler(test_cb{*this}, 42, _1, m1{})("s");

            // 4-ary
            bind_handler(test_cb{*this}, 42, "s")(m1{}, m2{});
            bind_handler(test_cb{*this}, 42, "s", m1{})(m2{});
            bind_handler(test_cb{*this}, 42, "s", m1{}, m2{})();
            bind_handler(test_cb{*this}, 42, _1, m1{})("s", m2{});
            bind_handler(test_cb{*this}, _3, _1, m1{})("s", m2{}, 42);
        }

        // perfect forwarding
        {
            std::shared_ptr<int> const sp =
                std::make_shared<int>(42);
            {
                bind_handler(test_cb{*this}, sp)();
                BEAST_EXPECT(sp.get() != nullptr);
            }
            {
                bind_handler(test_cb{*this})(sp);
                BEAST_EXPECT(sp.get() != nullptr);
            }
        }

        // associated executor
        {
            net::io_context ioc;
            testHooks(ioc, bind_handler(net::bind_executor(
                test_executor(*this, ioc), test_cb{*this})));
        }

        // asio_handler_invoke
        {
            // make sure things compile, also can set a
            // breakpoint in asio_handler_invoke to make sure
            // it is instantiated.
            net::io_context ioc;
            net::strand<
                net::io_context::executor_type> s{
                    ioc.get_executor()};
            net::post(s,
                bind_handler(test_cb{*this}, 42));
            ioc.run();
        }
    }

    void
    testBindFrontHandler()
    {
        using m1 = move_arg<1>;
        using m2 = move_arg<2>;

        // 0-ary
        bind_front_handler(test_cb{*this})();

        // 1-ary
        bind_front_handler(test_cb{*this}, 42)(); 
        bind_front_handler(test_cb{*this})(42);

        // 2-ary
        bind_front_handler(test_cb{*this}, 42, "s")();
        bind_front_handler(test_cb{*this}, 42)("s");
        bind_front_handler(test_cb{*this})(42, "s");

        // 3-ary
        bind_front_handler(test_cb{*this}, 42, "s", m1{})();
        bind_front_handler(test_cb{*this}, 42, "s")(m1{});
        bind_front_handler(test_cb{*this}, 42)("s", m1{});
        bind_front_handler(test_cb{*this})(42, "s", m1{});

        // 4-ary
        bind_front_handler(test_cb{*this}, 42, "s", m1{}, m2{})();
        bind_front_handler(test_cb{*this}, 42, "s", m1{})(m2{});
        bind_front_handler(test_cb{*this}, 42, "s")(m1{}, m2{});
        bind_front_handler(test_cb{*this}, 42)("s", m1{}, m2{});
        bind_front_handler(test_cb{*this})(42, "s", m1{}, m2{});

        error_code ec;
        std::size_t n = 256;
        
        // void(error_code, size_t)
        bind_front_handler(test_cb{*this}, ec, n)();

        // void(error_code, size_t)(string_view)
        bind_front_handler(test_cb{*this}, ec, n)("s");

        // perfect forwarding
        {
            std::shared_ptr<int> const sp =
                std::make_shared<int>(42);
            bind_front_handler(test_cb{*this}, sp)();
            BEAST_EXPECT(sp.get() != nullptr);
        }

        // associated executor
        {
            net::io_context ioc;

            testHooks(ioc, bind_front_handler(net::bind_executor(
                test_executor(*this, ioc), test_cb{*this})
                ));
            testHooks(ioc, bind_front_handler(net::bind_executor(
                test_executor(*this, ioc), test_cb{*this}),
                42));
            testHooks(ioc, bind_front_handler(net::bind_executor(
                test_executor(*this, ioc), test_cb{*this}),
                42, "s"));
            testHooks(ioc, bind_front_handler(net::bind_executor(
                test_executor(*this, ioc), test_cb{*this}),
                42, "s", m1{}));
            testHooks(ioc, bind_front_handler(net::bind_executor(
                test_executor(*this, ioc), test_cb{*this}),
                42, "s", m1{}, m2{}));
            testHooks(ioc, bind_front_handler(net::bind_executor(
                test_executor(*this, ioc), test_cb{*this}),
                ec, n));
        }
    }

    void
    run() override
    {
        testBindHandler();
        testBindFrontHandler();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,bind_handler);

} // beast
} // boost
