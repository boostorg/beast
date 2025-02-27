//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: HTTP client, asynchronous Unix domain sockets
//
//------------------------------------------------------------------------------

#include <boost/asio/local/stream_protocol.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#if defined(BOOST_ASIO_HAS_LOCAL_SOCKETS)

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using stream_protocol = net::local::stream_protocol; // from <boost/asio/local/stream_protocol.hpp>

//------------------------------------------------------------------------------

// Report a failure
void
fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Performs an HTTP GET and prints the response
class session : public std::enable_shared_from_this<session>
{
    beast::basic_stream<
        stream_protocol,
        net::any_io_executor,
        beast::unlimited_rate_policy> stream_;
    beast::flat_buffer buffer_; // (Must persist between reads)
    http::request<http::empty_body> req_;
    http::response<http::string_body> res_;

public:
    // Objects are constructed with a strand to
    // ensure that handlers do not execute concurrently.
    explicit
    session(net::io_context& ioc)
        : stream_(ioc)
    {
    }

    // Start the asynchronous operation
    void
    run(
        char const* path,
        char const* host,
        char const* port,
        char const* target,
        int version)
    {
        // Set up an HTTP GET request message
        req_.version(version);
        req_.method(http::verb::get);
        req_.target(target);
        req_.set(http::field::host, std::string{host} + ":" + port);
        req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        // Make the connection on the IP address we get from a lookup
        stream_.async_connect(
            stream_protocol::endpoint{path},
            beast::bind_front_handler(
                &session::on_connect,
                shared_from_this()));
    }

    void
    on_connect(beast::error_code ec)
    {
        if(ec)
            return fail(ec, "connect");

        // Set a timeout on the operation
        stream_.expires_after(std::chrono::seconds(30));

        // Send the HTTP request to the remote host
        http::async_write(stream_, req_,
            beast::bind_front_handler(
                &session::on_write,
                shared_from_this()));
    }

    void
    on_write(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
            return fail(ec, "write");
        
        // Receive the HTTP response
        http::async_read(stream_, buffer_, res_,
            beast::bind_front_handler(
                &session::on_read,
                shared_from_this()));
    }

    void
    on_read(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
            return fail(ec, "read");

        // Write the message to standard out
        std::cout << res_ << std::endl;

        // Gracefully close the socket
        stream_.socket().shutdown(stream_protocol::socket::shutdown_both, ec);

        // not_connected happens sometimes so don't bother reporting it.
        if(ec && ec != beast::errc::not_connected)
            return fail(ec, "shutdown");

        // If we get here then the connection is closed gracefully
    }
};

//------------------------------------------------------------------------------

int main(int argc, char** argv)
{
    // Check command line arguments.
    if(argc != 5 && argc != 6)
    {
        std::cerr <<
            "Usage: http-client-async-local <path> <host> <port> <target> [<HTTP version: 1.0 or 1.1(default)>]\n" <<
            "Example:\n" <<
            "    http-client-async-local /tmp/http.sock localhost 80 /\n" <<
            "    http-client-async-local /tmp/http.sock localhost 80 / 1.0\n";
        return EXIT_FAILURE;
    }
    auto const path = argv[1];
    auto const host = argv[2];
    auto const port = argv[3];
    auto const target = argv[4];
    int version = argc == 6 && !std::strcmp("1.0", argv[5]) ? 10 : 11;

    // The io_context is required for all I/O
    net::io_context ioc;

    // Launch the asynchronous operation
    std::make_shared<session>(ioc)->run(path, host, port, target, version);

    // Run the I/O service. The call will return when
    // the get operation is complete.
    ioc.run();

    return EXIT_SUCCESS;
}

#else

int
main(int, char*[])
{
std::cerr <<
    "Local sockets not available on this platform" << std::endl;
return EXIT_FAILURE;
}

#endif
