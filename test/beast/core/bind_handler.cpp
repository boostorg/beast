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
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <string>

namespace boost {
namespace beast {

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

    void
    callback(int v)
    {
        BEAST_EXPECT(v == 42);
    }

#if 0
    // This function should fail to compile
    void
    failStdBind()
    {
        std::bind(bind_handler(handler<>{}));
    }
    void
    failStdBindFront()
    {
        std::bind(bind_front_handler(handler<>{}));
    }
#endif
    
    void
    testBindHandler()
    {
        // invocation
        {
            auto f = bind_handler(std::bind(
                &bind_handler_test::callback, this,
                    std::placeholders::_1), 42);
            f();

        }

        // placeholders
        {
            namespace ph = std::placeholders;
            bind_handler(handler<>{})();
            bind_handler(handler<int>{}, 1)();
            bind_handler(handler<int, std::string>{}, 1, "hello")();
            bind_handler(handler<int>{}, ph::_1)(1);
            bind_handler(handler<int, std::string>{}, ph::_1, ph::_2)(1, "hello");
        }

        // move-only
        {
            bind_handler([](move_only){}, move_only{})();
            bind_handler([](move_only){}, std::placeholders::_1)(move_only{});
            bind_handler([](move_only, move_only){}, move_only{}, std::placeholders::_1)(move_only{});
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
            test::stream ts{ioc};
            net::post(s,
                bind_handler(copyable{}, 42));
        }
    }

    void
    testBindFrontHandler()
    {
        // invocation
        {
            bind_front_handler(
                std::bind(
                    &bind_handler_test::callback,
                    this,
                    42));

            bind_front_handler(
                std::bind(
                    &bind_handler_test::callback,
                    this,
                    std::placeholders::_1), 42);

            bind_front_handler(
                std::bind(
                    &bind_handler_test::callback,
                    this,
                    std::placeholders::_1))(42);

            bind_front_handler(
                bind_front_handler(
                    std::bind(
                        &bind_handler_test::callback,
                        this,
                        std::placeholders::_1)),
                42);

            bind_front_handler(
                bind_front_handler(
                    std::bind(
                        &bind_handler_test::callback,
                        this,
                        std::placeholders::_1)))(42);
        }

        // move-only
        {
            bind_front_handler([]{});
        }
        
        // specializations
        {
            bind_front_handler(copyable{});
            bind_front_handler(copyable{}, 1);
            bind_front_handler(copyable{}, 1, 2);
            bind_front_handler(copyable{}, 1, 2, 3);
            bind_front_handler(copyable{}, 1, 2, 3, 4);

            bind_front_handler(copyable{},
                error_code{}, std::size_t(4));
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
            test::stream ts{ioc};
            net::post(s,
                bind_front_handler(copyable{}, 42));
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
