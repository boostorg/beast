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
request(ssl::context& ctx)
{
    auto executor = co_await net::this_coro::executor;
    net::ip::tcp::endpoint endpoint{ {}, 8080 };
    ssl::stream<net::ip::tcp::socket> stream{ executor, ctx };

    // Connect TCP socket
    co_await stream.lowest_layer().async_connect(endpoint);

    // Set Server Name Indication (SNI)
    if(!SSL_set_tlsext_host_name(stream.native_handle(), "localhost"))
    {
        throw beast::system_error(
            static_cast<int>(::ERR_get_error()),
            net::error::get_ssl_category());
    }

    // Set a callback to verify that the hostname in the server
    // certificate matches the expected value
    stream.set_verify_callback(ssl::host_name_verification("localhost"));

    // Perform the SSL handshake
    co_await stream.async_handshake(ssl::stream_base::client);

    // Write an HTTP GET request
    http::request<http::empty_body> req{ http::verb::get, "/", 11 };
    req.set(http::field::host, "localhost");
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    co_await http::async_write(stream, req);

    // Read the response
    beast::flat_buffer buf;
    http::response<http::string_body> res;
    co_await http::async_read(stream, buf, res);

    // Print the response body
    std::cout << res.body();

    // Gracefully shutdown the SSL stream
    auto [ec] = co_await stream.async_shutdown(net::as_tuple);
    if(ec && ec != ssl::error::stream_truncated)
        throw boost::system::system_error(ec);
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

        // set up the peer verification mode so that the TLS/SSL handshake fails
        // if the certificate verification is unsuccessful
        ctx.set_verify_mode(ssl::verify_peer);

        // The servers's certificate will be verified against this
        // certificate authority.
        ctx.load_verify_file("ca.crt");

        // In a real application, the passphrase would be read from
        // a secure place, such as a key vault.
        ctx.set_password_callback([](auto, auto) { return "123456"; });

        // Client certificate and private key (if server request for).
        ctx.use_certificate_chain_file("client.crt");
        ctx.use_private_key_file("client.key", ssl::context::pem);

        net::co_spawn(ioc, request(ctx), print_exception);

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
