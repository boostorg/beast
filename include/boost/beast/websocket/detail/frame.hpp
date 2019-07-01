//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_DETAIL_FRAME_HPP
#define BOOST_BEAST_WEBSOCKET_DETAIL_FRAME_HPP

#include <boost/beast/core/buffer_traits.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/core/detail/buffers_pair.hpp>
#include <cstdint>

namespace boost {
namespace beast {
namespace websocket {
namespace detail {

// frame header opcodes
enum class opcode : std::uint8_t
{
    cont    = 0,
    text    = 1,
    binary  = 2,
    rsv3    = 3,
    rsv4    = 4,
    rsv5    = 5,
    rsv6    = 6,
    rsv7    = 7,
    close   = 8,
    ping    = 9,
    pong    = 10,
    crsvb   = 11,
    crsvc   = 12,
    crsvd   = 13,
    crsve   = 14,
    crsvf   = 15
};

// Contents of a WebSocket frame header
struct frame_header
{
    std::uint64_t len;
    std::uint32_t key;
    opcode op;
    bool fin  : 1;
    bool mask : 1;
    bool rsv1 : 1;
    bool rsv2 : 1;
    bool rsv3 : 1;
};

// holds the largest possible frame header
using fh_buffer = flat_static_buffer<14>;

// holds the largest possible control frame
using frame_buffer =
    flat_static_buffer< 2 + 8 + 4 + 125 >;

inline
bool constexpr
is_reserved(opcode op)
{
    return
        (op >= opcode::rsv3  && op <= opcode::rsv7) ||
        (op >= opcode::crsvb && op <= opcode::crsvf);
}

inline
bool constexpr
is_valid(opcode op)
{
    return op <= opcode::crsvf;
}

inline
bool constexpr
is_control(opcode op)
{
    return op >= opcode::close;
}

BOOST_BEAST_DECL
bool
is_valid_close_code(std::uint16_t v);

//------------------------------------------------------------------------------

// Write frame header to dynamic buffer
//
BOOST_BEAST_DECL
void
write(flat_static_buffer_base& db, frame_header const& fh);

// Read data from buffers
// This is for ping and pong payloads
//
BOOST_BEAST_DECL
void
read_ping(
    ping_data& data,
    beast::buffers_prefix_view<beast::detail::buffers_pair<true>> const& bs);

// Read close_reason, return true on success
// This is for the close payload
//
BOOST_BEAST_DECL
void
read_close(
    close_reason& cr,
    beast::buffers_prefix_view<beast::detail::buffers_pair<true>> const& bs,
    error_code& ec);

} // detail
} // websocket
} // beast
} // boost

#if BOOST_BEAST_HEADER_ONLY
#include <boost/beast/websocket/detail/frame.ipp>
#endif

#endif
