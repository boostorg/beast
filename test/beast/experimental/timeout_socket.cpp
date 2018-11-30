//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/_experimental/core/timeout_socket.hpp>

#include <boost/beast/_experimental/core/timeout_service.hpp>

#include <boost/beast/test/yield_to.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <thread>

namespace boost {
namespace beast {

class timeout_socket_test
    : public beast::unit_test::suite
    , public test::enable_yield_to
{
public:
    class server
    {
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
            net::ip::tcp::endpoint ep,
            std::ostream& log)
            : log_(log)
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
                [this](error_code ec){ this->on_accept(ec); });

            t_ = std::thread([this]{ ioc_.run(); });
        }

        ~server()
        {
            ioc_.stop();
            t_.join();
        }

    private:
        class session
            : public std::enable_shared_from_this<session>
        {
            net::ip::tcp::socket socket_;
        
        public:
            session(
                net::ip::tcp::socket sock,
                std::ostream&)
                : socket_(std::move(sock))
            {
            }

            void
            run()
            {
                socket_.async_wait(
                    net::socket_base::wait_read,
                    std::bind(
                        &session::on_read,
                        shared_from_this(),
                        std::placeholders::_1));
            }

        protected:
            void
            on_read(error_code ec)
            {
                boost::ignore_unused(ec);
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
                    std::move(socket_), log_)->run();
            acceptor_.async_accept(socket_,
                [this](error_code ec)
                {
                    this->on_accept(ec);
                });
        }
    };

    void
    testAsync()
    {
        net::ip::tcp::endpoint ep(
            net::ip::make_address("127.0.0.1"), 8080);
        server srv(ep, log);
        {
            net::io_context ioc;
            set_timeout_service_options(
                ioc, std::chrono::seconds(1));
            timeout_socket s(ioc);
            s.next_layer().connect(ep);
            char buf[32];
            s.async_read_some(net::buffer(buf),
                [&](error_code ec, std::size_t n)
                {
                    log << "read_some: " << ec.message() << "\n";
                    boost::ignore_unused(ec, n);
                });
            ioc.run();
        }
    }

    void
    run()
    {
        testAsync();

        pass();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,timeout_socket);

} // beast
} // boost
