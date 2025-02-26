//
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>

#include <iostream>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
namespace ssl   = net::ssl;

void
print_exception(std::exception_ptr eptr)
{
    if(eptr)
    {
        try
        {
            std::rethrow_exception(eptr);
        }
        catch(std::exception& e)
        {
            std::cerr << e.what() << std::endl;
        }
    }
}

net::awaitable<void>
handle_session(ssl::stream<net::ip::tcp::socket> stream)
{
    // Perform the SSL handshake
    co_await stream.async_handshake(ssl::stream_base::server);

    // Read and discard a request
    beast::flat_buffer buf;
    http::request<http::empty_body> req;
    co_await http::async_read(stream, buf, req);

    // Write the response
    http::response<http::string_body> res;
    res.body() = "Hello!";
    co_await http::async_write(stream, res);

    // Gracefully shutdown the SSL stream
    auto [ec] = co_await stream.async_shutdown(net::as_tuple);
    if(ec && ec != ssl::error::stream_truncated)
        throw boost::system::system_error(ec);
}

net::awaitable<void>
acceptor(ssl::context& ctx)
{
    auto executor = co_await net::this_coro::executor;
    net::ip::tcp::endpoint endpoint{ {}, 8080 };
    net::ip::tcp::acceptor acceptor{ executor, endpoint };

    for(;;)
    {
        net::co_spawn(
            executor,
            handle_session({ co_await acceptor.async_accept(), ctx }),
            print_exception);
    }
}

int
main()
{
    try
    {
        // The io_context is required for all I/O
        net::io_context ioc;

        // The SSL context is required, and holds certificates,
        // configurations and session related data
        ssl::context ctx{ ssl::context::sslv23 };

        // https://docs.openssl.org/3.4/man3/SSL_CTX_set_options/
        ctx.set_options(
            ssl::context::no_sslv2 | ssl::context::default_workarounds |
            ssl::context::single_dh_use);

        // Comment this line to disable client certificate request.
        ctx.set_verify_mode(
            ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);

        // The client's certificate will be verified against this
        // certificate authority.
        ctx.load_verify_file("ca.crt");

        // In a real application, the passphrase would be read from
        // a secure place, such as a key vault.
        ctx.set_password_callback([](auto, auto) { return "123456"; });

        // Server certificate and private key.
        ctx.use_certificate_chain_file("server.crt");
        ctx.use_private_key_file("server.key", ssl::context::pem);

        // DH parameters for DHE-based cipher suites
        ctx.use_tmp_dh_file("dh4096.pem");

        net::co_spawn(ioc, acceptor(ctx), print_exception);

        ioc.run();
    }
    catch(std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
}

#else

int
main(int, char*[])
{
    std::printf("awaitables require C++20\n");
    return EXIT_FAILURE;
}

#endif
