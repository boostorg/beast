#include "json_body.hpp"
#include <boost/json/src.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <iostream>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>
using tcp = net::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

int main(int, char **)
{
    // Our test endpoint for testing the json
    const auto host = "postman-echo.com";
    const auto target = "/post";

    // The io_context is required for all I/O
    net::io_context ioc;

    // These objects perform our I/O
    tcp::resolver resolver(ioc);
    beast::tcp_stream stream(ioc);

    // Look up the domain name
    auto const results = resolver.resolve(host, "80");

    // Make the connection on the IP address we get from a lookup
    stream.connect(results);

    // Set up an HTTP POST request message
    http::request<json_body> req{http::verb::post, target, 11};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.set(http::field::content_type, "application/json");
    req.body() = {{"type", "test"}, {"content", "pure awesomeness"}};
    req.prepare_payload();
    // Send the HTTP request to the remote host
    http::write(stream, req);
    
    // This buffer is used for reading and must be persisted
    beast::flat_buffer buffer;

    // Get the response
    http::response<json_body> res;

    // Receive the HTTP response
    http::read(stream, buffer, res);

    // Write the message to standard out
    std::cout << res << std::endl;

    // Gracefully close the socket
    beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);


    return 0;
}
