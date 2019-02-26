//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <boost/beast/core.hpp>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/use_future.hpp>
#include <algorithm>
#include <future>
#include <iostream>
#include <thread>

using namespace boost::beast;

#include <boost/beast/websocket.hpp>
using namespace boost::beast::websocket;

namespace doc_ws_snippets {

void fxx() {

net::io_context ioc;
auto work = net::make_work_guard(ioc);
std::thread t{[&](){ ioc.run(); }};
error_code ec;
net::ip::tcp::socket sock{ioc};
boost::ignore_unused(ec);

{
//[ws_snippet_3
    stream<net::ip::tcp::socket> ws{std::move(sock)};
//]
}

{
//[ws_snippet_4
    stream<net::ip::tcp::socket&> ws{sock};
//]

//[ws_snippet_5
    ws.next_layer().shutdown(net::ip::tcp::socket::shutdown_send);
//]
}

{
//[ws_snippet_6
    std::string const host = "example.com";
    net::ip::tcp::resolver r(ioc);
    stream<tcp_stream> ws(ioc);
    auto const results = r.resolve(host, "ws");
    get_lowest_layer(ws).connect(results.begin(), results.end());
//]
}

{
//[ws_snippet_7
    net::ip::tcp::acceptor acceptor{ioc};
    stream<tcp_stream> ws{acceptor.get_executor()};
    acceptor.accept(get_lowest_layer(ws).socket());
//]
}

{
    stream<net::ip::tcp::socket> ws{ioc};
//[ws_snippet_15
    // This DynamicBuffer will hold the received message
    multi_buffer buffer;

    // Read a complete message into the buffer's input area
    ws.read(buffer);

    // Set text mode if the received message was also text,
    // otherwise binary mode will be set.
    ws.text(ws.got_text());

    // Echo the received message back to the peer. If the received
    // message was in text mode, the echoed message will also be
    // in text mode, otherwise it will be in binary mode.
    ws.write(buffer.data());

    // Discard all of the bytes stored in the dynamic buffer,
    // otherwise the next call to read will append to the existing
    // data instead of building a fresh message.
    buffer.consume(buffer.size());
//]
}

{
    stream<net::ip::tcp::socket> ws{ioc};
//[ws_snippet_16
    // This DynamicBuffer will hold the received message
    multi_buffer buffer;

    // Read the next message in pieces
    do
    {
        // Append up to 512 bytes of the message into the buffer
        ws.read_some(buffer, 512);
    }
    while(! ws.is_message_done());

    // At this point we have a complete message in the buffer, now echo it

    // The echoed message will be sent in binary mode if the received
    // message was in binary mode, otherwise we will send in text mode.
    ws.binary(ws.got_binary());

    // This buffer adaptor allows us to iterate through buffer in pieces
    buffers_suffix<multi_buffer::const_buffers_type> cb{buffer.data()};

    // Echo the received message in pieces.
    // This will cause the message to be broken up into multiple frames.
    for(;;)
    {
        if(buffer_size(cb) > 512)
        {
            // There are more than 512 bytes left to send, just
            // send the next 512 bytes. The value `false` informs
            // the stream that the message is not complete.
            ws.write_some(false, buffers_prefix(512, cb));

            // This efficiently discards data from the adaptor by
            // simply ignoring it, but does not actually affect the
            // underlying dynamic buffer.
            cb.consume(512);
        }
        else
        {
            // Only 512 bytes or less remain, so write the whole
            // thing and inform the stream that this piece represents
            // the end of the message by passing `true`.
            ws.write_some(true, cb);
            break;
        }
    }

    // Discard all of the bytes stored in the dynamic buffer,
    // otherwise the next call to read will append to the existing
    // data instead of building a fresh message.
//]
}

{
    stream<net::ip::tcp::socket> ws{ioc};
//[ws_snippet_17
    ws.control_callback(
        [](frame_type kind, string_view payload)
        {
            // Do something with the payload
            boost::ignore_unused(kind, payload);
        });
//]

//[ws_snippet_18
    ws.close(close_code::normal);
//]

//[ws_snippet_19
    ws.auto_fragment(true);
    ws.write_buffer_size(16384);
//]

//[ws_snippet_20
    multi_buffer buffer;
    ws.async_read(buffer,
        [](error_code, std::size_t)
        {
            // Do something with the buffer
        });
//]

{
    multi_buffer b;
//[ws_snippet_24
    ws.async_read(b, [](error_code, std::size_t){});
    ws.async_read(b, [](error_code, std::size_t){});
//]
}

{
    multi_buffer b;
//[ws_snippet_25
    ws.async_read(b, [](error_code, std::size_t){});
    ws.async_write(b.data(), [](error_code, std::size_t){});
    ws.async_ping({}, [](error_code){});
    ws.async_close({}, [](error_code){});
//]
}

}

} // fxx()

// workaround for https://github.com/chriskohlhoff/asio/issues/112
#ifdef BOOST_MSVC
//[ws_snippet_21
void echo(stream<net::ip::tcp::socket>& ws,
    multi_buffer& buffer, net::yield_context yield)
{
    ws.async_read(buffer, yield);
    std::future<std::size_t> fut =
        ws.async_write(buffer.data(), net::use_future);
}
//]
#endif

//[ws_snippet_22

struct custom_stream;

void
teardown(
    role_type role,
    custom_stream& stream,
    error_code& ec);

template<class TeardownHandler>
void
async_teardown(
    role_type role,
    custom_stream& stream,
    TeardownHandler&& handler);

//]

//[ws_snippet_23

template<class NextLayer>
struct custom_wrapper
{
    NextLayer next_layer;

    template<class... Args>
    explicit
    custom_wrapper(Args&&... args)
        : next_layer(std::forward<Args>(args)...)
    {
    }

    friend
    void
    teardown(
        role_type role,
        custom_wrapper& stream,
        error_code& ec)
    {
        using boost::beast::websocket::teardown;
        teardown(role, stream.next_layer, ec);
    }

    template<class TeardownHandler>
    friend
    void
    async_teardown(
        role_type role,
        custom_wrapper& stream,
        TeardownHandler&& handler)
    {
        using boost::beast::websocket::async_teardown;
        async_teardown(role, stream.next_layer, std::forward<TeardownHandler>(handler));
    }
};

//]

} // doc_ws_snippets

//------------------------------------------------------------------------------

namespace doc_wss_snippets {

void fxx() {

net::io_context ioc;
auto work = net::make_work_guard(ioc);
std::thread t{[&](){ ioc.run(); }};
error_code ec;
net::ip::tcp::socket sock{ioc};

{
//[wss_snippet_3
    net::ip::tcp::endpoint ep;
    net::ssl::context ctx{net::ssl::context::sslv23};
    stream<net::ssl::stream<net::ip::tcp::socket>> ws{ioc, ctx};

    // connect the underlying TCP/IP socket
    ws.next_layer().next_layer().connect(ep);

    // perform SSL handshake
    ws.next_layer().handshake(net::ssl::stream_base::client);

    // perform WebSocket handshake
    ws.handshake("localhost", "/");
//]
}

} // fxx()

} // doc_wss_snippets
