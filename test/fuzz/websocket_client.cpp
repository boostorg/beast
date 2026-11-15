//
// Copyright (c) 2026 the Boost.Beast fuzz target authors
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// WebSocket client-session fuzzer.
//
// Threat model: a Beast websocket CLIENT talking to an attacker-run server.
// The existing websocket_server target covers the accept/parse side; this
// target exists for the client side, which no target exercises today: the
// handshake request generation and response validation, masked outgoing
// frames (mask key PRNG), permessage-deflate negotiation on both ends,
// compressed writes and reads, ping/pong and the close handshake.
//
// Mechanics: the harness runs the peer server role on a second thread over
// connected test::stream sockets, so the Sec-WebSocket-Accept value the
// client validates is genuinely computed from the client's own random key.
// The fuzz input controls the message payload bytes and an option matrix
// (pmd enablement on each side, window bits, compression level, echo, text
// vs binary, close code).
//
// Memory safety of the harness: message payloads are capped slices of the
// fuzz input, every read buffer is a capped flat_buffer, and all negotiated
// parameters are clamped to the documented valid ranges, so no allocation
// is driven by an unvalidated length.
//

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/_experimental/test/stream.hpp>

#include <cstdint>
#include <string>
#include <thread>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace test = boost::beast::test;

namespace {

constexpr std::size_t kMaxBuf = 256 * 1024;  // cap for every read buffer
constexpr std::size_t kMaxMsg = 16 * 1024;   // cap for written payloads

// Flags byte (last input byte): option matrix.
enum flags_t : std::uint8_t
{
    fl_cli_pmd  = 0x01,  // client offers permessage-deflate
    fl_srv_pmd  = 0x02,  // server accepts permessage-deflate
    fl_echo     = 0x04,  // server echoes the client message back
    fl_binary   = 0x08,  // client sends a binary message
    fl_goaway   = 0x10,  // close with going_away instead of normal
};

void
server_peer(net::io_context& ioc,
    test::stream&& sock,
    std::uint8_t flags,
    std::uint8_t ctl)
{
    beast::error_code ec;
    websocket::stream<test::stream> ws{std::move(sock)};
    websocket::permessage_deflate pd;
    pd.server_enable = (flags & fl_srv_pmd) != 0;
    pd.server_max_window_bits = 9 + (ctl % 7);  // documented valid range 9..15
    pd.compLevel = ctl % 9;
    ws.set_option(pd);
    ws.accept(ec);
    std::string const m1 = ec.message();  // exercise the error tables
    (void)m1;
    if(ec)
        return;

    beast::flat_buffer buf(kMaxBuf);
    ws.read(buf, ec);  // auto-pongs pings, consumes messages
    std::string const m2 = ec.message();
    (void)m2;
    if(ec)
        return;

    if(flags & fl_echo)
    {
        ws.text(ws.got_text());
        ws.write(buf.data(), ec);
        std::string const m3 = ec.message();
        (void)m3;
    }
    else
    {
        // always send something so the client's read never blocks
        ws.write(net::const_buffer{"s", 1}, ec);
        std::string const m3 = ec.message();
        (void)m3;
    }

    // drain the client's close frame
    beast::flat_buffer buf2(kMaxBuf);
    ws.read(buf2, ec);
    std::string const m4 = ec.message();
    (void)m4;
}

} // namespace

extern "C"
int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if(size < 3)
        return 0;
    const std::uint8_t flags = data[size - 1];
    const std::uint8_t ctl = data[size - 2];
    const std::size_t payload_len =
        (size - 2) < kMaxMsg ? (size - 2) : kMaxMsg;
    const char* payload = reinterpret_cast<const char*>(data);
    net::io_context ioc;

    test::stream ss{ioc};  // attacker server end
    test::stream cs{ioc};  // client end
    cs.connect(ss);

    std::thread srv([&]
        { server_peer(ioc, std::move(ss), flags, ctl); });

    beast::error_code ec;
    {
        websocket::stream<test::stream> ws{std::move(cs)};
        websocket::permessage_deflate pd;
        pd.client_enable = (flags & fl_cli_pmd) != 0;
        pd.client_max_window_bits = 9 + ((ctl >> 3) % 7);
        ws.set_option(pd);

        // generates the handshake request with a random key (base64-encodes
        // it) and validates the attacker-supplied response
        ws.handshake("example.com", "/", ec);
        std::string const m0 = ec.message();
        (void)m0;
        if(!ec)
        {
            ws.text((flags & fl_binary) == 0);
            // masked write: mask keys come from the PRNG (chacha) path
            ws.write(net::const_buffer{payload, payload_len}, ec);
            std::string const m1 = ec.message();
            (void)m1;
            if(!ec && payload_len > 0)
            {
                std::size_t ping_len = ctl & 15;
                if(ping_len > payload_len)
                    ping_len = payload_len;
                ws.ping(
                    websocket::ping_data{payload, ping_len}, ec);
                std::string const m2 = ec.message();
                (void)m2;
            }
            beast::flat_buffer rbuf(kMaxBuf);
            ws.read(rbuf, ec);  // echo (possibly inflated) or filler byte
            std::string const m3 = ec.message();
            (void)m3;
            ws.close(
                (flags & fl_goaway)
                    ? websocket::close_code::going_away
                    : websocket::close_code::normal,
                ec);
            std::string const m4 = ec.message();
            (void)m4;
        }
    }
    srv.join();
    return 0;
}
