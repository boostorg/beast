//
// Copyright (c) 2021-2022 Guilherme Schvarcz Franco (guilhermefrancosi at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: HTTP client, synchronous, POST with payload data
//
//------------------------------------------------------------------------------

//[example_http_client_post

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/json.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace http = beast::http;       // from <boost/beast/http.hpp>
namespace net = boost::asio;        // from <boost/asio.hpp>
using tcp = net::ip::tcp;           // from <boost/asio/ip/tcp.hpp>

struct parameters {
    explicit parameters(int argc, char** argv);
    http::message_generator make_request() const;

    char const* host() const { return args_.at(1); }
    char const* port() const { return args_.at(2); }
private:
    char const* program_name() const { return args_.at(0); }
    char const* target() const { return args_.at(3); }
    int version() const { return !strcmp("1.0", args_.at(5)) ? 10 : 11; }

    std::array<char const*, 6> args_{{
        "http-client-sync-post",
        "www.example.com",
        "80",
        "/",
        "formdata",
        "1.1",
    }};
};

/*explicit*/ parameters::parameters(int argc, char** argv)
{
    args_.front() = argv[0];
    if(argc < 4 || argc > 6)
    {
        std::cerr
            << "Usage: " << program_name()
            << R"( <host> <port> <target> [<Payload type>] [<HTTP version>]

Payload type:
    "formdata" (default)
    "json"
    <file> (uses multipart/formdata)

HTTP version:
    1.0
    1.1 (default)

Example:)"
            << "\n    " << program_name() << " www.example.com 80 /"
            << "\n    " << program_name()
            << " www.example.com 80 /upload \"path/to/document.pdf\" 1.0\n";
        ::exit(EXIT_FAILURE);
    }
    std::copy(argv + 0, argv + argc, args_.begin());
}

http::message_generator parameters::make_request() const
{
    // Set up an HTTP POST request message with chosen payload type
    http::request<http::string_body> req{http::verb::post, target(), version()};
    req.set(http::field::host, host());
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    std::ostringstream payload;

    beast::string_view arg(args_.at(4));
    if(arg == "formdata")
    {
        req.set(http::field::content_type, "application/x-www-form-urlencoded");
        payload << "var1=1&var2=2";
    } else if(arg == "json")
    {
        payload << boost::json::value{"m_list", {1, 2, 3}};
        req.set(http::field::content_type, "application/json");
    } else
    {
        // Prepare the multipart/form-data message
        boost::filesystem::path fileName(arg);

#define MULTI_PART_BOUNDARY                                                    \
        "AaB03x" // This is the boundary to limit the start/end of a
                 // part. It may be any string. More info on the RFC
                 // 2388
                 // (https://datatracker.ietf.org/doc/html/rfc2388)
#define CRLF                                                                   \
        "\r\n" // Line ends must be CRLF
               // https://datatracker.ietf.org/doc/html/rfc7231#section-3.1.1.4

        payload
            << "--" MULTI_PART_BOUNDARY CRLF
            << R"(Content-Disposition: form-data; name="comment")"
            << CRLF CRLF "Larry" CRLF << //
            "--" MULTI_PART_BOUNDARY CRLF
            << R"(Content-Disposition: form-data; name="files"; filename=)"
            << fileName.filename() << CRLF
            << "Content-Type: application/octet-stream" CRLF CRLF
            << std::ifstream(fileName.native(), std::ios::binary).rdbuf()
            << CRLF << //
            "--" MULTI_PART_BOUNDARY << "--" CRLF;

        req.set(
                http::field::content_type,
                "multipart/form-data; boundary=" MULTI_PART_BOUNDARY);

#undef MULTI_PART_BOUNDARY
#undef CRLF
    }

    // Allow Beast to set headers depending on HTTP version, verb and body
    req.body() = std::move(payload).str();
    req.prepare_payload();
    return req;
}

// Performs an HTTP POST and prints the response
int main(int argc, char** argv)
{
    try
    {
        parameters cmd(argc, argv);

        // The io_context is required for all I/O
        net::io_context ioc;

        // These objects perform our I/O
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);

        // Look up the domain name
        auto const results = resolver.resolve(cmd.host(), cmd.port());

        // Make the connection on the IP address we get from a lookup
        stream.connect(results);

        auto req = cmd.make_request();

        // Send the HTTP request to the remote host
        beast::write(stream, req);

        // This buffer is used for reading and must be persisted
        beast::flat_buffer buffer;

        // Declare a container to hold the response
        http::response<http::dynamic_body> res;

        // Receive the HTTP response
        http::read(stream, buffer, res);

        // Write the message to standard out
        std::cout << res << std::endl;

        // Gracefully close the socket
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        // not_connected happens sometimes
        // so don't bother reporting it.
        //
        if(ec && ec != beast::errc::not_connected)
            throw beast::system_error{ec};

        // If we get here then the connection is closed gracefully
    }
    catch(std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

//]
