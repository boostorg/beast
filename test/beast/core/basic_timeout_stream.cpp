//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/basic_timeout_stream.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/timeout_stream.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>
#include <array>
#include <thread>

namespace boost {
namespace beast {

class basic_timeout_stream_test
    : public beast::unit_test::suite
{
public:
    //--------------------------------------------------------------------------

    struct socket_pair
    {
        net::io_context ioc1;
        net::ip::tcp::socket s1;

        net::io_context ioc2;
        net::ip::tcp::socket s2;

        socket_pair()
            : s1(ioc1)
            , s2(ioc2)
        {
            net::io_context ioc;
            net::ip::tcp::acceptor a(ioc);
            net::ip::tcp::endpoint ep(
                net::ip::make_address_v4("127.0.0.1"), 0);
            a.open(ep.protocol());
            a.set_option(
                net::socket_base::reuse_address(true));
            a.bind(ep);
            a.listen(1);
            a.async_accept(s2,
                [](error_code ec)
                {
                #if 0
                    if(ec == net::error::operation_aborted)
                        return;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                #endif
                });
            s1.async_connect(a.local_endpoint(),
                [](error_code ec)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                });
            for(;;)
                if(
                    ioc.poll() +
                    ioc1.poll() +
                    ioc2.poll() == 0)
                    break;
            BOOST_ASSERT(s1.is_open());
        #if 0
            BOOST_ASSERT(s2.is_open()); // VFALCO Fails on Travis for some reason
            BOOST_ASSERT(
                s1.remote_endpoint() ==
                s2.local_endpoint());
            BOOST_ASSERT(
                s2.remote_endpoint() ==
                s1.local_endpoint());
        #endif
        }
    };

    //--------------------------------------------------------------------------

    class server
    {
        string_view s_;
        std::ostream& log_;
        net::io_context ioc_;
        net::ip::tcp::acceptor acceptor_;
        net::ip::tcp::socket socket_;
        std::thread t_;

        void
        fail(error_code ec, string_view what)
        {
            if(ec != net::error::operation_aborted)
                log_ << what << ": " << ec.message() << "\n";
        }

    public:
        server(
            string_view s,
            net::ip::tcp::endpoint ep,
            std::ostream& log)
            : s_(s)
            , log_(log)
            , ioc_(1)
            , acceptor_(ioc_)
            , socket_(ioc_)
        {
            boost::system::error_code ec;

            acceptor_.open(ep.protocol(), ec);
            if(ec)
            {
                fail(ec, "open");
                return;
            }

            acceptor_.set_option(
                net::socket_base::reuse_address(true), ec);
            if(ec)
            {
                fail(ec, "set_option");
                return;
            }

            acceptor_.bind(ep, ec);
            if(ec)
            {
                fail(ec, "bind");
                return;
            }

            acceptor_.listen(
                net::socket_base::max_listen_connections, ec);
            if(ec)
            {
                fail(ec, "listen");
                return;
            }

            acceptor_.async_accept(socket_,
                [this](error_code ec)
                {
                    this->on_accept(ec);
                });

            t_ = std::thread(
                [this]
                {
                    ioc_.run();
                });
        }

        ~server()
        {
            ioc_.stop();
            t_.join();
        }

        net::ip::tcp::endpoint
        local_endpoint() const noexcept
        {
            return acceptor_.local_endpoint();
        }

    private:
        class session
            : public std::enable_shared_from_this<session>
        {
            string_view s_;
            net::ip::tcp::socket socket_;
        
        public:
            session(
                string_view s,
                net::ip::tcp::socket sock,
                std::ostream&)
                : s_(s)
                , socket_(std::move(sock))
            {
            }

            void
            run()
            {
                if(s_.empty())
                    socket_.async_wait(
                        net::socket_base::wait_read,
                        std::bind(
                            &session::on_read,
                            shared_from_this(),
                            std::placeholders::_1));
                else
                    net::async_write(
                        socket_,
                        net::const_buffer(s_.data(), s_.size()),
                        std::bind(
                            &session::on_write,
                            shared_from_this(),
                            std::placeholders::_1,
                            std::placeholders::_2));
            }

        protected:
            void
            on_read(error_code ec)
            {
                boost::ignore_unused(ec);
            }

            void
            on_write(error_code, std::size_t)
            {
            }
        };

        void
        on_accept(error_code ec)
        {
            if(! acceptor_.is_open())
                return;
            if(ec)
                fail(ec, "accept");
            else
                std::make_shared<session>(
                    s_, std::move(socket_), log_)->run();
            acceptor_.async_accept(socket_,
                [this](error_code ec)
                {
                    this->on_accept(ec);
                });
        }
    };

    //--------------------------------------------------------------------------

    void
    testStrand()
    {
        {
            using strand_type = net::io_context::strand;
            net::io_context ioc;
            strand_type st(ioc);
            basic_timeout_stream<
                net::ip::tcp, strand_type> s(st);
            BEAST_EXPECT(s.get_executor() == st);
        }

    #if 0
        // VFALCO This is disallowed until Asio implements P1322R0
        {
            using strand_type = net::strand<
                net::io_context::executor_type>;
            net::io_context ioc;
            strand_type st(ioc.get_executor());
            basic_timeout_stream<
                net::ip::tcp, strand_type> s(st);
            BEAST_EXPECT(s.get_executor() == st);
        }
    #endif
    }

    struct other_t
    {
    };

    void
    testMembers()
    {
        using tcp = net::ip::tcp;
        using stream_t = basic_timeout_stream<tcp>;

        net::io_context ioc;
        auto ex = ioc.get_executor();

        // construction

        BOOST_STATIC_ASSERT(! std::is_constructible<
            stream_t, other_t>::value);

        BOOST_STATIC_ASSERT(! std::is_constructible<
            stream_t, other_t, tcp::socket>::value);

        {
            stream_t s(ioc);
        }

        {
            stream_t s(ex);
        }

        {
            stream_t s((tcp::socket(ioc)));
        }

        {
            stream_t s(ex, tcp::socket(ioc));
        }

        {
            net::io_context ioc2;
            try
            {
                // mismatched execution contexts
                stream_t s(
                    ioc2.get_executor(),
                    tcp::socket(ioc));
                fail("mismatched execution context", __FILE__, __LINE__);
            }
            catch(std::invalid_argument const&)
            {
                pass();
            }
        }

        // move

        {
            stream_t s1(ioc);
            stream_t s2(std::move(s1));
        }

        // assign

        {
            stream_t s1(ioc);
            stream_t s2(ioc);
            s2 = std::move(s1);
        }

        // get_executor

        {
            stream_t s(ioc);
            BEAST_EXPECT(
                s.get_executor() == ioc.get_executor());
        }

        // layers

        {
            net::socket_base::keep_alive opt;
            tcp::socket sock(ioc);
            sock.open(tcp::v4());
            sock.get_option(opt);
            BEAST_EXPECT(! opt.value());
            stream_t s(ioc);
            s.next_layer().open(tcp::v4());
            s.next_layer().get_option(opt);
            BEAST_EXPECT(! opt.value());
            opt = true;
            sock.set_option(opt);
            opt = false;
            BEAST_EXPECT(! opt.value());
            s = stream_t(std::move(sock));
            static_cast<stream_t const&>(s).next_layer().get_option(opt);
            BEAST_EXPECT(opt.value());
        }
    }

    //--------------------------------------------------------------------------

    struct match
    {
        suite& suite_;
        error_code ec_;
        std::size_t n_;

        match(suite& s, error_code ec, std::size_t n)
            : suite_(s)
            , ec_(ec)
            , n_(n)
        {
        }

        match(match&& other)
            : suite_(other.suite_)
            , ec_(other.ec_)
            , n_(boost::exchange(other.n_,
                (std::numeric_limits<std::size_t>::max)()))
        {
        }

        ~match()
        {
            suite_.BEAST_EXPECT(
                n_ == (std::numeric_limits<std::size_t>::max)());
        }

        void
        operator()(error_code ec, std::size_t n)
        {
            suite_.expect(ec == ec_, ec.message(), __FILE__, __LINE__);
            suite_.BEAST_EXPECT(n == n_);
            n_ = (std::numeric_limits<std::size_t>::max)();
        }
    };
    
    void
    testRead()
    {
        using tcp = net::ip::tcp;
        using stream_t = basic_timeout_stream<tcp>;

        char buf[4];
        std::memset(buf, 0, 4);
        net::mutable_buffer mb(buf, sizeof(buf));
        auto const ep = net::ip::tcp::endpoint(
            net::ip::make_address("127.0.0.1"), 0);

        // success
        {
            server srv("*", ep, log);
            net::io_context ioc;
            stream_t s(ioc);
            s.next_layer().connect(srv.local_endpoint());
            s.async_read_some(mb, match{*this, {}, 1});
            ioc.run_for(std::chrono::seconds(1));
        }

        // success, with timeout
        {
            server srv("*", ep, log);
            net::io_context ioc;
            stream_t s(ioc);
            s.next_layer().connect(srv.local_endpoint());
            s.expires_after(std::chrono::milliseconds(100));
            s.async_read_some(mb, match{*this, {}, 1});
            ioc.run_for(std::chrono::seconds(1));
            s.expires_never();
            ioc.run();
        }

        // close
        {
            server srv("", ep, log);
            net::io_context ioc;
            stream_t s(ioc);
            s.next_layer().connect(srv.local_endpoint());
            s.async_read_some(mb, match{*this,
                net::error::operation_aborted, 0});
            {
                error_code ec;
                s.next_layer().shutdown(
                    net::socket_base::shutdown_both,
                    ec);
            }
            s.close();
            ioc.run_for(std::chrono::seconds(1));
        }

        // cancel
        {
            server srv("", ep, log);
            net::io_context ioc;
            stream_t s(ioc);
            s.next_layer().connect(srv.local_endpoint());
            s.async_read_some(mb, match{*this,
                net::error::operation_aborted, 0});
            ioc.run_for(std::chrono::milliseconds(100));
            s.cancel();
            ioc.run_for(std::chrono::seconds(1));
        }

        // immediate timeout
        {
            server srv("*", ep, log);
            net::io_context ioc;
            stream_t s(ioc);
            s.next_layer().connect(srv.local_endpoint());
            s.expires_after(std::chrono::seconds(-1));
            s.async_read_some(mb,
                [&](error_code ec, std::size_t n)
                {
                #if 0
                    // Unreliable on epoll impls
                    BEAST_EXPECT(
                        (ec == error::timeout && n == 0) ||
                        (! ec && n == 1));
                #else
                    boost::ignore_unused(ec, n);
                    pass();
                #endif
                });
            ioc.run_for(std::chrono::seconds(1));
        }

        // fail, with timeout
        {
            server srv("", ep, log);
            net::io_context ioc;
            stream_t s(ioc);
            s.next_layer().connect(srv.local_endpoint());
            s.expires_after(std::chrono::milliseconds(100));
            s.async_read_some(mb,
                match{*this, error::timeout, 0});
            ioc.run_for(std::chrono::seconds(1));
        }

        // success, with timeout
        {
            server srv("*", ep, log);
            net::io_context ioc;
            stream_t s(ioc);
            s.next_layer().connect(srv.local_endpoint());
            s.expires_at(
                std::chrono::steady_clock::now() +
                std::chrono::milliseconds(100));
            s.async_read_some(mb,
                match{*this, {}, 1});
            ioc.run_for(std::chrono::seconds(1));
        }

        // abandoned ops
        {
            server srv("*", ep, log);
            net::io_context ioc;
            stream_t s(ioc);
            s.next_layer().connect(srv.local_endpoint());
            s.async_read_some(mb, [&](error_code, std::size_t){});
        }
        {
            server srv("*", ep, log);
            net::io_context ioc;
            stream_t s(ioc);
            s.next_layer().connect(srv.local_endpoint());
            s.expires_after(std::chrono::seconds(1));
            s.async_read_some(mb, [&](error_code, std::size_t){});
        }

        // edge case:
        // timer completion becomes queued before
        // the I/O completion handler is invoked
        // VFALCO Fails on OSX Travis
#if 0
        {
            socket_pair p;
            bool invoked = false;
            stream_t s(std::move(p.s1));
            s.expires_after(std::chrono::seconds(0));
            s.async_read_some(mb,
                [&](error_code ec, std::size_t)
                {
                    invoked = true;
                    BEAST_EXPECTS(ec == error::timeout,
                        ec.message());
                });
            p.s2.async_write_some(
                net::const_buffer("*", 1),
                [&](error_code ec, std::size_t n)
                {
                    boost::ignore_unused(ec, n);
                });
            p.ioc1.run();
            p.ioc1.restart();
            p.ioc2.run();
            p.ioc2.restart();
            p.ioc1.run();
            BEAST_EXPECT(invoked);
        }
#endif
    }

    void
    testWrite()
    {
        using tcp = net::ip::tcp;
        using stream_t = basic_timeout_stream<tcp>;

        char buf[4];
        std::memset(buf, 0, 4);
        net::mutable_buffer mb(buf, sizeof(buf));
        auto const ep = net::ip::tcp::endpoint(
            net::ip::make_address("127.0.0.1"), 0);

        // write
        {
            server srv("", ep, log);
            net::io_context ioc;
            stream_t s(ioc);
            s.next_layer().connect(srv.local_endpoint());
            s.async_write_some(mb,
                match{*this, {}, mb.size()});
            {
                error_code ec;
                s.next_layer().shutdown(
                    net::socket_base::shutdown_both,
                    ec);
            }
            s.close();
            ioc.run();
        }

        // write abandoned
        {
            server srv("*", ep, log);
            net::io_context ioc;
            stream_t s(ioc);
            s.next_layer().connect(srv.local_endpoint());
            s.async_write_some(mb, [&](error_code, std::size_t){});
        }
    }

    void
    testConnect()
    {
        using tcp = net::ip::tcp;
        using stream_t = basic_timeout_stream<tcp>;

        auto const ep = net::ip::tcp::endpoint(
            net::ip::make_address("127.0.0.1"), 0);

        // overload 1
        {
            server srv("", ep, log);
            net::io_context ioc;
            stream_t s(ioc);
            bool invoked = false;
            std::array<tcp::endpoint, 1> epa{{
                srv.local_endpoint()}};
            beast::async_connect(s, epa,
                [&](error_code ec, tcp::endpoint)
                {
                    invoked = true;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            ioc.run_for(std::chrono::seconds(1));
            BEAST_EXPECT(invoked);
        }

        // overload 2
        {
            server srv("", ep, log);
            net::io_context ioc;
            stream_t s(ioc);
            bool invoked = false;
            std::array<tcp::endpoint, 1> epa{{
                srv.local_endpoint()}};
            beast::async_connect(s, epa,
                [](error_code, tcp::endpoint)
                {
                    return true;
                },
                [&](error_code ec, tcp::endpoint)
                {
                    invoked = true;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            ioc.run_for(std::chrono::seconds(1));
            BEAST_EXPECT(invoked);
        }

        // overload 3
        {
            server srv("", ep, log);
            net::io_context ioc;
            stream_t s(ioc);
            bool invoked = false;
            std::array<tcp::endpoint, 1> epa{{
                srv.local_endpoint()}};
            using iter_type =
                std::array<tcp::endpoint, 1>::const_iterator;
            beast::async_connect(s, epa.begin(), epa.end(),
                [&](error_code ec, iter_type)
                {
                    invoked = true;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            ioc.run_for(std::chrono::seconds(1));
            BEAST_EXPECT(invoked);
        }

        // overload 4
        {
            server srv("", ep, log);
            net::io_context ioc;
            stream_t s(ioc);
            bool invoked = false;
            std::array<tcp::endpoint, 1> epa{{
                srv.local_endpoint()}};
            using iter_type =
                std::array<tcp::endpoint, 1>::const_iterator;
            beast::async_connect(s, epa.begin(), epa.end(),
                [](error_code, tcp::endpoint)
                {
                    return true;
                },
                [&](error_code ec, iter_type)
                {
                    invoked = true;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            ioc.run_for(std::chrono::seconds(1));
            BEAST_EXPECT(invoked);
        }

        // success
        {
            server srv("", ep, log);
            net::io_context ioc;
            stream_t s(ioc);
            bool invoked = false;
            std::array<tcp::endpoint, 1> epa{{
                srv.local_endpoint()}};
            beast::async_connect(s, epa,
                [&](error_code ec, tcp::endpoint)
                {
                    invoked = true;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            ioc.run_for(std::chrono::seconds(1));
            BEAST_EXPECT(invoked);
        }

        // success, with timeout
        {
            server srv("", ep, log);
            net::io_context ioc;
            stream_t s(ioc);
            bool invoked = false;
            std::array<tcp::endpoint, 1> epa{{
                srv.local_endpoint()}};
            s.expires_after(std::chrono::milliseconds(100));
            beast::async_connect(s, epa,
                [&](error_code ec, tcp::endpoint)
                {
                    invoked = true;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            ioc.run_for(std::chrono::seconds(1));
            BEAST_EXPECT(invoked);
        }

        // immediate timeout
        {
            net::io_context ioc;
            stream_t s(tcp::socket(ioc, tcp::v6()));
            bool invoked = false;
            std::array<tcp::endpoint, 1> epa{{
                net::ip::tcp::endpoint(
                    net::ip::make_address("192.168.0.254"), 1)}};
            s.expires_after(std::chrono::seconds(-1));
            beast::async_connect(s, epa,
                [&](error_code ec, tcp::endpoint)
                {
                    invoked = true;
                    BEAST_EXPECTS(
                        ec == error::timeout, ec.message());
                });
            ioc.run_for(std::chrono::seconds(1));
            BEAST_EXPECT(invoked);
        }

        // edge case:
        // timer completion becomes queued before
        // the I/O completion handler is invoked
        // VFALCO Seems to hang on OSX Travis
#if 0
        {
            net::io_context ioc1;
            stream_t s1(ioc1);
            net::io_context ioc2;
            net::ip::tcp::acceptor a(ioc2);
            a.open(ep.protocol());
            a.set_option(
                net::socket_base::reuse_address(true));
            a.bind(ep);
            a.listen(1);
            a.async_accept([](error_code, tcp::socket){});
            bool invoked = false;
            s1.expires_after(std::chrono::seconds(0));
            s1.async_connect(
                a.local_endpoint(),
                [&](error_code ec)
                {
                    invoked = true;
                    BEAST_EXPECTS(ec == error::timeout,
                        ec.message());
                });
            ioc1.run();
            ioc1.restart();
            ioc2.run();
            ioc2.restart();
            ioc1.run();
            BEAST_EXPECT(invoked);
        }
#endif

        /*  VFALCO
            We need a reliable way of causing a real
            timeout, for example a stable IP address
            for which connections are never established,
            but that also do not cause immediate failure.
        */
#if 0
        // timeout (unreachable ipv4 host)
        {
            net::io_context ioc;
            stream_t s(tcp::socket(ioc, tcp::v6()));
            bool invoked = false;
            std::array<tcp::endpoint, 1> epa{{
                net::ip::tcp::endpoint(
                    net::ip::make_address("192.168.0.254"), 1)}};
            s.expires_after(std::chrono::milliseconds(100));
            beast::async_connect(s, epa,
                [&](error_code ec, tcp::endpoint)
                {
                    invoked = true;
                    BEAST_EXPECTS(
                        ec == error::timeout, ec.message());
                });
            ioc.run_for(std::chrono::seconds(1));
            BEAST_EXPECT(invoked);
        }

        // timeout (ipv6 black hole)
        {
            net::io_context ioc;
            stream_t s(tcp::socket(ioc, tcp::v6()));
            bool invoked = false;
            std::array<tcp::endpoint, 1> epa{{
                tcp::endpoint(
                    net::ip::address(
                        net::ip::make_address_v6("100::")),
                    1)
                }};
            s.expires_after(std::chrono::milliseconds(100));
            beast::async_connect(s, epa,
                [&](error_code ec, tcp::endpoint)
                {
                    invoked = true;
                    BEAST_EXPECTS(
                        ec == error::timeout, ec.message());
                });
            ioc.run_for(std::chrono::seconds(1));
            BEAST_EXPECT(invoked);
        }
#endif
    }

    //--------------------------------------------------------------------------

    http::response<http::string_body>
    make_response(http::request<http::empty_body>)
    {
        return {};
    }

    void process_http_1 (timeout_stream& stream, net::yield_context yield)
    {
        flat_buffer buffer;
        http::request<http::empty_body> req;

        // Read the request, with a 15 second timeout
        stream.expires_after(std::chrono::seconds(15));
        http::async_read(stream, buffer, req, yield);

        // Calculate the response
        http::response<http::string_body> res = make_response(req);

        // Send the response, with a 30 second timeout.
        stream.expires_after (std::chrono::seconds(30));
        http::async_write (stream, res, yield);
    }

    void process_http_2 (timeout_stream& stream, net::yield_context yield)
    {
        flat_buffer buffer;
        http::request<http::empty_body> req;

        // Require that the read and write combined take no longer than 30 seconds
        stream.expires_after(std::chrono::seconds(30));

        http::async_read(stream, buffer, req, yield);

        http::response<http::string_body> res = make_response(req);
        http::async_write (stream, res, yield);
    }

    websocket::stream<timeout_stream>
    process_websocket(timeout_stream&& stream, net::yield_context yield)
    {
        websocket::stream<timeout_stream> ws(std::move(stream));

        // Require that the entire websocket handshake take no longer than 10 seconds
        ws.next_layer().expires_after(std::chrono::seconds(10));
        ws.async_accept(yield);

        return ws;
    }

    void
    testJavadocs()
    {
        BEAST_EXPECT(&basic_timeout_stream_test::process_http_1);
        BEAST_EXPECT(&basic_timeout_stream_test::process_http_2);
        BEAST_EXPECT(&basic_timeout_stream_test::process_websocket);
    }

    //--------------------------------------------------------------------------

    void
    run()
    {
        testStrand();
        testMembers();
        testRead();
        testWrite();
        testConnect();
        testJavadocs();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,basic_timeout_stream);

} // beast
} // boost
