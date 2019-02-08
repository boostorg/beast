//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/stranded_socket.hpp>

#include "stream_tests.hpp"

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <memory>

namespace boost {
namespace beast {

namespace {

template<class Executor = net::io_context::executor_type>
class test_executor
{
public:
    // VFALCO These need to be atomic or something
    struct info
    {
        int dispatch = 0;
        int post = 0;
        int defer = 0;
        int work = 0;
        int total = 0;
    };

private:
    struct state
    {
        Executor ex;
        info info_;

        state(Executor const& ex_)
            : ex(ex_)
        {
        }
    };

    std::shared_ptr<state> sp_;

public:
    test_executor(test_executor const&) = default;
    test_executor& operator=(test_executor const&) = default;

    explicit
    test_executor(Executor const& ex)
        : sp_(std::make_shared<state>(ex))
    {
    }

    decltype(sp_->ex.context())
    context() const noexcept
    {
        return sp_->ex.context();
    }

    info&
    operator*() noexcept
    {
        return sp_->info_;
    }

    info*
    operator->() noexcept
    {
        return &sp_->info_;
    }

    void
    on_work_started() const noexcept
    {
        ++sp_->info_.work;
    }

    void
    on_work_finished() const noexcept
    {
    }

    template<class F, class A>
    void
    dispatch(F&& f, A const& a)
    {
        ++sp_->info_.dispatch;
        ++sp_->info_.total;
        sp_->ex.dispatch(
            std::forward<F>(f), a);
    }

    template<class F, class A>
    void
    post(F&& f, A const& a)
    {
        ++sp_->info_.post;
        ++sp_->info_.total;
        sp_->ex.post(
            std::forward<F>(f), a);
    }

    template<class F, class A>
    void
    defer(F&& f, A const& a)
    {
        ++sp_->info_.defer;
        ++sp_->info_.total;
        sp_->ex.defer(
            std::forward<F>(f), a);
    }
};

struct test_handler
{
    int& flags;

    void
    operator()()
    {
        flags |= 1;
    }

    template<class F>
    friend
    void
    asio_handler_invoke(F&& f, test_handler* p)
    {
        p->flags |= 2;
        std::move(f)();
    }
};

struct test_acceptor
{
    net::io_context ioc;
    net::ip::tcp::acceptor a;
    net::ip::tcp::endpoint ep;

    test_acceptor()
        : a(ioc)
        , ep(net::ip::make_address_v4("127.0.0.1"), 0)
    {
        a.open(ep.protocol());
        a.set_option(
            net::socket_base::reuse_address(true));
        a.bind(ep);
        a.listen(1);
        ep = a.local_endpoint();
        a.async_accept(
            [](error_code, net::ip::tcp::socket)
            {
            });
    }
};

} // (anon)

//------------------------------------------------------------------------------

class stranded_socket_test
    : public beast::unit_test::suite
{
public:
    using tcp = net::ip::tcp;
    using strand = net::io_context::strand;
    using executor = net::io_context::executor_type;

    void
    testStream()
    {
        net::io_context ioc;

        // default Executor

        {
            stranded_socket<tcp> s1{strand(ioc)};
            stranded_socket<tcp> s2{strand{ioc}};
            //stranded_socket<tcp> s3{strand{ioc}}; // ambiguous parse
        }

        // explicit Executor

        {
            auto ex = ioc.get_executor();
            stranded_socket<tcp, executor> s1(ioc);
            stranded_socket<tcp, executor> s2(ex);
            stranded_socket<tcp, executor> s3(ioc, tcp::v4());
            stranded_socket<tcp, executor> s4(std::move(s1));
            s2.socket() = tcp::socket(ioc);
            BEAST_EXPECT(s1.get_executor() == ex);
            BEAST_EXPECT(s2.get_executor() == ex);
            BEAST_EXPECT(s3.get_executor() == ex);
            BEAST_EXPECT(s4.get_executor() == ex);

            BEAST_EXPECT((! static_cast<
                stranded_socket<tcp, executor> const&>(
                    s2).socket().is_open()));
        }

        {
            auto ex = strand{ioc};
            stranded_socket<tcp, strand> s1(ex);
            stranded_socket<tcp, strand> s2(ex, tcp::v4());
            stranded_socket<tcp, strand> s3(std::move(s1));
            s2.socket() = tcp::socket(ioc);
            BEAST_EXPECT(s1.get_executor() == ex);
            BEAST_EXPECT(s2.get_executor() == ex);
            BEAST_EXPECT(s3.get_executor() == ex);

            BEAST_EXPECT((! static_cast<
                stranded_socket<tcp, strand> const&>(
                    s2).socket().is_open()));
        }

        {
            test_sync_stream<stranded_socket<tcp, executor>>();
            test_async_stream<stranded_socket<tcp, executor>>();
            test_sync_stream<stranded_socket<tcp, strand>>();
            test_async_stream<stranded_socket<tcp, strand>>();
        }
    }

    void
    testMembers()
    {
        net::io_context ioc;

        // connect (member)

        auto const cond =
            [](error_code, tcp::endpoint)
            {
                return true;
            };

        {
            stranded_socket<tcp, executor> s(ioc);
            error_code ec;
            test_acceptor a;
            try
            {
                s.connect(a.ep);
                BEAST_PASS();
            }
            catch(std::exception const&)
            {
                BEAST_FAIL();
            }
        }

        {
            stranded_socket<tcp, executor> s(ioc);
            error_code ec;
            test_acceptor a;
            s.connect(a.ep, ec);
            BEAST_EXPECT(! ec);
        }

        // connect

        {
            test_acceptor a;
            std::array<tcp::endpoint, 1> epa;
            epa[0] = a.ep;
            stranded_socket<tcp, executor> s(ioc);
            error_code ec;
            connect(s, epa);
            connect(s, epa, ec);
        }

        {
            test_acceptor a;
            std::array<tcp::endpoint, 1> epa;
            epa[0] = a.ep;
            stranded_socket<tcp, executor> s(ioc);
            error_code ec;
            connect(s, epa, cond);
            connect(s, epa, cond, ec);
        }

        {
            test_acceptor a;
            std::array<tcp::endpoint, 1> epa;
            epa[0] = a.ep;
            stranded_socket<tcp, executor> s(ioc);
            error_code ec;
            connect(s, epa.begin(), epa.end());
            connect(s, epa.begin(), epa.end(), ec);
        }

        {
            test_acceptor a;
            std::array<tcp::endpoint, 1> epa;
            epa[0] = a.ep;
            stranded_socket<tcp, executor> s(ioc);
            error_code ec;
            connect(s, epa.begin(), epa.end(), cond);
            connect(s, epa.begin(), epa.end(), cond, ec);
        }

        // async_connect

        {
            stranded_socket<tcp, executor> s(ioc);
            test_acceptor a;
            error_code ec;
            s.async_connect(a. ep,
                [](error_code ec)
                {
                    BEAST_EXPECT(! ec);
                });
            ioc.run();
            ioc.restart();
        }

        {
            std::array<tcp::endpoint, 1> epa;
            epa[0] = tcp::endpoint(
                net::ip::make_address_v4("127.0.0.1"), 0);
            stranded_socket<tcp, executor> s(ioc);
            async_connect(s, epa,
                [](error_code, tcp::endpoint)
                {
                });
        }

        {
            std::array<tcp::endpoint, 1> epa;
            epa[0] = tcp::endpoint(
                net::ip::make_address_v4("127.0.0.1"), 0);
            stranded_socket<tcp, executor> s(ioc);
            async_connect(s, epa, cond,
                [](error_code, tcp::endpoint)
                {
                });
        }

        {
            std::array<tcp::endpoint, 1> epa;
            epa[0] = tcp::endpoint(
                net::ip::make_address_v4("127.0.0.1"), 0);
            using iter_type = decltype(epa)::const_iterator;
            stranded_socket<tcp, executor> s(ioc);
            async_connect(s, epa.begin(), epa.end(),
                [](error_code, iter_type)
                {
                });
        }

        {
            std::array<tcp::endpoint, 1> epa;
            epa[0] = tcp::endpoint(
                net::ip::make_address_v4("127.0.0.1"), 0);
            using iter_type = decltype(epa)::const_iterator;
            stranded_socket<tcp, executor> s(ioc);
            async_connect(s, epa.begin(), epa.end(), cond,
                [](error_code, iter_type)
                {
                });
        }

        // read/write

        {
            error_code ec;
            stranded_socket<tcp, executor> s(ioc, tcp::v4());

            BEAST_EXPECT(s.read_some(net::mutable_buffer{}) == 0);
            BEAST_EXPECT(s.read_some(net::mutable_buffer{}, ec) == 0);
            BEAST_EXPECTS(! ec, ec.message());

            BEAST_EXPECT(s.write_some(net::const_buffer{}) == 0);
            BEAST_EXPECT(s.write_some(net::const_buffer{}, ec) == 0);
            BEAST_EXPECTS(! ec, ec.message());

            bool invoked;

            invoked = false;
            s.async_read_some(net::mutable_buffer{},
                [&](error_code ec, std::size_t)
                {
                    invoked = true;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(invoked);

            invoked = false;
            s.async_write_some(net::const_buffer{},
                [&](error_code ec, std::size_t)
                {
                    invoked = true;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(invoked);
        }

        // stranded

        {
            error_code ec;
            stranded_socket<tcp, strand> s(strand(ioc), tcp::v4());

            bool invoked;

            invoked = false;
            s.async_read_some(net::mutable_buffer{},
                [&](error_code ec, std::size_t)
                {
                    invoked = true;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(invoked);

            invoked = false;
            s.async_write_some(net::const_buffer{},
                [&](error_code ec, std::size_t)
                {
                    invoked = true;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(invoked);
        }

        // test_executor

        {
            error_code ec;
            stranded_socket<tcp, test_executor<>> s(
                test_executor<>(ioc.get_executor()), tcp::v4());

            bool invoked;

            invoked = false;
            s.async_read_some(net::mutable_buffer{},
                [&](error_code ec, std::size_t)
                {
                    invoked = true;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(invoked);
            BEAST_EXPECT(s.get_executor()->total > 0);
            s.get_executor()->total = 0;

            invoked = false;
            s.async_write_some(net::const_buffer{},
                [&](error_code ec, std::size_t)
                {
                    invoked = true;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(invoked);
            BEAST_EXPECT(s.get_executor()->total > 0);
            s.get_executor()->total = 0;
        }

        // bind_default_executor::asio_handler_invoke

#if 0
        // VFALCO This test fails, because it is unclear how
        //        asio_handler_invoke interacts with the wrapper.
        //        Need to ask Chris Kohlhoff about this one.
        {
            int flags = 0;
            net::post(
                ioc,
                detail::bind_default_executor(
                    strand(ioc),
                    test_handler{flags}));
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(flags == 3);
        }
#endif
    }

    //--------------------------------------------------------------------------

    void
    testJavadocs()
    {
    }

    //--------------------------------------------------------------------------

    void
    run()
    {
        testStream();
        testJavadocs();
        testMembers();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,stranded_socket);

} // beast
} // boost
