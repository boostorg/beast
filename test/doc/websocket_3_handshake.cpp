//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include "snippets.hpp"

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/core/async_op_base.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/async_result.hpp>
#include <cstdlib>
#include <utility>

namespace boost {
namespace beast {
namespace websocket {

namespace {

void
websocket_3_handshake_snippets()
{
    #include "snippets.ipp"
    stream<net::ip::tcp::socket> ws(ioc);
    {
    //[code_websocket_3_client_1

        // Note that the stream must already be connected, this
        // function does not perform a DNS lookup on the host
        // name, nor does it establish an outgoing connection.

        // Perform the websocket handshake in the client role.
        ws.handshake(
            "www.example.com",  // The Host field
            "/",                // The request-target
            ec                  // Set to the error
        );

    //]
    }
    {
    //[code_websocket_3_client_2

        // Note that the stream must already be connected, this
        // function does not perform a DNS lookup on the host
        // name, nor does it establish an outgoing connection.

        // This variable will receive the HTTP response from the server
        response_type res;

        // Perform the websocket handshake in the client role.
        ws.handshake(
            res,                // Receives the HTTP response
            "www.example.com",  // The Host field
            "/"                 // The request-target
            ,ec                 // Set to the error, if any
        );

        // Upon success, `res` will be set to the complete
        // response received from the server.
    //]
    }

    //--------------------------------------------------------------------------

    {
    //[code_websocket_3_server_1

        // Note that the stream must already be connected
        // to an incoming remote peer.

        // Perform the websocket handshake in the server role.
        ws.accept(
            ec                  // Set to the error, if any
        );

    //]
    }
    {
    //[code_websocket_3_server_2
        // This buffer is required for reading HTTP messages
        flat_buffer buffer;

        // Read into our buffer until we reach the end of the HTTP request.
        // No parsing takes place here, we are just accumulating data.
        // We use beast::dynamic_buffer_ref to pass a lightweight, movable
        // reference to our buffer, because Networking expects to take
        // ownership unlike Beast algorithms which use a reference.

        net::read_until(sock, dynamic_buffer_ref(buffer), "\r\n\r\n");

        // Now accept the connection, using the buffered data.
        ws.accept(buffer.data());
    //]
    }
}

void
websocket_3_handshake_snippets_2()
{
    using namespace boost::beast;
    namespace net = boost::asio;
    namespace ssl = boost::asio::ssl;
    using tcp     = net::ip::tcp;
    error_code ec;
    net::io_context ioc;
    tcp::socket sock(ioc);

    {
    //[code_websocket_3_server_1b
        // This buffer is required for reading HTTP messages
        flat_buffer buffer;

        // Read the HTTP request ourselves
        http::request<http::string_body> req;
        http::read(sock, buffer, req);

        // See if its a WebSocket upgrade request
        if(websocket::is_upgrade(req))
        {
            // Construct the stream, transferring ownership of the socket
            stream<net::ip::tcp::socket> ws{std::move(sock)};

            // Clients SHOULD NOT begin sending WebSocket
            // frames until the server has provided a response.
            BOOST_ASSERT(buffer.size() == 0);

            // Accept the upgrade request
            ws.accept(req);
        }
        else
        {
            // Its not a WebSocket upgrade, so
            // handle it like a normal HTTP request.
        }
    //]
    }
}

//[code_websocket_3_decorator_1b
void set_user_agent(request_type& req)
{
    // Set the User-Agent on the request
    req.set(http::field::user_agent, "My User Agent");
}
//]

void
websocket_3_handshake_snippets_3()
{
    #include "snippets.ipp"
    stream<net::ip::tcp::socket> ws(ioc);
    {
    //[code_websocket_3_decorator_1

        ws.set_option(stream_base::decorator(&set_user_agent));

    //]
    }
    {
    //[code_websocket_3_decorator_2

        struct set_server
        {
            void operator()(response_type& res)
            {
                // Set the Server field on the response
                res.set(http::field::user_agent, "My Server");
            }
        };

        ws.set_option(stream_base::decorator(set_server{}));

    //]
    }
    {
    //[code_websocket_3_decorator_3

        ws.set_option(stream_base::decorator(
            [](response_type& res)
            {
                // Set the Server field on the response
                res.set(http::field::user_agent, "My Server");
            }));

    //]
    }
    //[code_websocket_3_decorator_4

        struct multi_decorator
        {
            void operator()(request_type& req)
            {
                // Set the User-Agent on the request
                req.set(http::field::user_agent, "My User Agent");
            }

            void operator()(response_type& res)
            {
                // Set the Server field on the response
                res.set(http::field::user_agent, "My Server");
            }
        };

        ws.set_option(stream_base::decorator(multi_decorator{}));

    //]
}

} // (anon)

struct websocket_3_handshake_test
    : public beast::unit_test::suite
{
    void
    run() override
    {
        BEAST_EXPECT(&websocket_3_handshake_snippets);
        BEAST_EXPECT(&websocket_3_handshake_snippets_2);
        BEAST_EXPECT(&websocket_3_handshake_snippets_3);
    }
};

BEAST_DEFINE_TESTSUITE(beast,doc,websocket_3_handshake);

} // websocket
} // beast
} // boost
