//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <beast/http/message_v1.hpp>

#include <beast/unit_test/suite.hpp>
#include <beast/unit_test/thread.hpp>
#include <beast/core/placeholders.hpp>
#include <beast/core/streambuf.hpp>
#include <beast/http.hpp>
#include <boost/asio.hpp>

namespace beast {
namespace http {

class sync_echo_http_server
{
public:
    using endpoint_type = boost::asio::ip::tcp::endpoint;
    using address_type = boost::asio::ip::address;
    using socket_type = boost::asio::ip::tcp::socket;

private:
    unit_test::suite& suite_;
    boost::asio::io_service ios_;
    socket_type sock_;
    boost::asio::ip::tcp::acceptor acceptor_;
    unit_test::thread thread_;

public:
    sync_echo_http_server(
            endpoint_type ep, unit_test::suite& suite)
        : suite_(suite)
        , sock_(ios_)
        , acceptor_(ios_)
    {
        error_code ec;
        acceptor_.open(ep.protocol(), ec);
        maybe_throw(ec, "open");
        acceptor_.bind(ep, ec);
        maybe_throw(ec, "bind");
        acceptor_.listen(
            boost::asio::socket_base::max_connections, ec);
        maybe_throw(ec, "listen");
        acceptor_.async_accept(sock_,
            std::bind(&sync_echo_http_server::on_accept, this,
                beast::asio::placeholders::error));
        thread_ = unit_test::thread(suite_,
            [&]
            {
                ios_.run();
            });
    }

    ~sync_echo_http_server()
    {
        error_code ec;
        ios_.dispatch(
            [&]{ acceptor_.close(ec); });
        thread_.join();
    }

private:
    void
    fail(error_code ec, std::string what)
    {
        suite_.log <<
            what << ": " << ec.message();
    }

    void
    maybe_throw(error_code ec, std::string what)
    {
        if(ec &&
            ec != boost::asio::error::operation_aborted)
        {
            fail(ec, what);
            throw ec;
        }
    }

    void
    on_accept(error_code ec)
    {
        if(ec == boost::asio::error::operation_aborted)
            return;
        maybe_throw(ec, "accept");
        std::thread{&sync_echo_http_server::do_client, this,
            std::move(sock_), boost::asio::io_service::work{
                sock_.get_io_service()}}.detach();
        acceptor_.async_accept(sock_,
            std::bind(&sync_echo_http_server::on_accept, this,
                beast::asio::placeholders::error));
    }

    void
    do_client(socket_type sock, boost::asio::io_service::work)
    {
        error_code ec;
        streambuf rb;
        for(;;)
        {
            request_v1<string_body> req;
            read(sock, rb, req, ec);
            if(ec)
                break;
            response_v1<string_body> resp;
            resp.status = 100;
            resp.reason = "OK";
            resp.version = req.version;
            resp.body = "Completed successfully.";
            write(sock, resp, ec);
            if(ec)
                break;
        }
    }
};

class message_v1_test : public beast::unit_test::suite
{
public:
    using endpoint_type = boost::asio::ip::tcp::endpoint;
    using address_type = boost::asio::ip::address;
    using socket_type = boost::asio::ip::tcp::socket;

    void testFunctions()
    {
        request_v1<empty_body> m;
        m.version = 10;
        expect(! is_upgrade(m));
        m.headers.insert("Transfer-Encoding", "chunked");
        try
        {
            prepare(m);
            fail();
        }
        catch(std::exception const&)
        {
        }
        m.headers.erase("Transfer-Encoding");
        m.headers.insert("Content-Length", "0");
        try
        {
            prepare(m);
            fail();
        }
        catch(std::exception const&)
        {
        }
        m.headers.erase("Content-Length");
        m.headers.insert("Connection", "keep-alive");
        try
        {
            prepare(m);
            fail();
        }
        catch(std::exception const&)
        {
        }
    }

    void
    syncEcho(endpoint_type ep)
    {
        boost::asio::io_service ios;
        socket_type sock(ios);
        sock.connect(ep);

        streambuf rb;
        {
            request_v1<string_body> req;
            req.method = "GET";
            req.url = "/";
            req.version = 11;
            req.body = "Beast.HTTP";
            req.headers.replace("Host",
                ep.address().to_string() + ":" +
                    std::to_string(ep.port()));
            write(sock, req);
        }
        {
            response_v1<string_body> m;
            read(sock, rb, m);
        }
    }

    void
    testAsio()
    {
        endpoint_type ep{
            address_type::from_string("127.0.0.1"), 6000};
        sync_echo_http_server s(ep, *this);
        syncEcho(ep);
    }

    void run() override
    {
        testFunctions();
        testAsio();
        pass();
    }
};

BEAST_DEFINE_TESTSUITE(message_v1,http,beast);

} // http
} // beast
