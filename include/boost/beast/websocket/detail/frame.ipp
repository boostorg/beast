//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_DETAIL_FRAME_IPP
#define BOOST_BEAST_WEBSOCKET_DETAIL_FRAME_IPP

#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/detail/frame.hpp>
#include <boost/beast/websocket/detail/utf8_checker.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/assert.hpp>
#include <boost/endian/conversion.hpp>
#include <cstdint>

namespace boost {
namespace beast {
namespace websocket {
namespace detail {

bool
is_valid_close_code(std::uint16_t v)
{
    switch(v)
    {
    case close_code::normal:            // 1000
    case close_code::going_away:        // 1001
    case close_code::protocol_error:    // 1002
    case close_code::unknown_data:      // 1003
    case close_code::bad_payload:       // 1007
    case close_code::policy_error:      // 1008
    case close_code::too_big:           // 1009
    case close_code::needs_extension:   // 1010
    case close_code::internal_error:    // 1011
    case close_code::service_restart:   // 1012
    case close_code::try_again_later:   // 1013
        return true;

    // explicitly reserved
    case close_code::reserved1:         // 1004
    case close_code::no_status:         // 1005
    case close_code::abnormal:          // 1006
    case close_code::reserved2:         // 1014
    case close_code::reserved3:         // 1015
        return false;
    }
    // reserved
    if(v >= 1016 && v <= 2999)
        return false;
    // not used
    if(v <= 999)
        return false;
    return true;
}

//------------------------------------------------------------------------------

void
write(flat_static_buffer_base& db, frame_header const& fh)
{
    std::size_t n;
    std::uint8_t b[14];
    b[0] = (fh.fin ? 0x80 : 0x00) | static_cast<std::uint8_t>(fh.op);
    if(fh.rsv1)
        b[0] |= 0x40;
    if(fh.rsv2)
        b[0] |= 0x20;
    if(fh.rsv3)
        b[0] |= 0x10;
    b[1] = fh.mask ? 0x80 : 0x00;
    if(fh.len <= 125)
    {
        b[1] |= fh.len;
        n = 2;
    }
    else if(fh.len <= 65535)
    {
        b[1] |= 126;
        auto len_be = endian::native_to_big(
            static_cast<std::uint16_t>(fh.len));
        std::memcpy(&b[2], &len_be, sizeof(len_be));
        n = 4;
    }
    else
    {
        b[1] |= 127;
        auto len_be = endian::native_to_big(
            static_cast<std::uint64_t>(fh.len));
        std::memcpy(&b[2], &len_be, sizeof(len_be));
        n = 10;
    }
    if(fh.mask)
    {
        auto key_le = endian::native_to_little(
            static_cast<std::uint32_t>(fh.key));
        std::memcpy(&b[n], &key_le, sizeof(key_le));
        n += 4;
    }
    db.commit(net::buffer_copy(
        db.prepare(n), net::buffer(b)));
}

void
read_ping(ping_data& data, beast::buffers_prefix_view<beast::detail::buffers_pair<true>> const& bs)
{
    BOOST_ASSERT(buffer_bytes(bs) <= data.max_size());
    data.resize(buffer_bytes(bs));
    net::buffer_copy(net::mutable_buffer{
        data.data(), data.size()}, bs);
}

void
read_close(
    close_reason& cr,
    beast::buffers_prefix_view<beast::detail::buffers_pair<true>> const& bs,
    error_code& ec)
{
    auto const n = buffer_bytes(bs);
    BOOST_ASSERT(n <= 125);
    if(n == 0)
    {
        cr = close_reason{};
        ec = {};
        return;
    }
    if(n == 1)
    {
        // invalid payload size == 1
        ec = error::bad_close_size;
        return;
    }

    std::uint16_t code_be;
    cr.reason.resize(n - 2);
    std::array<net::mutable_buffer, 2> out_bufs{{
        net::mutable_buffer(&code_be, sizeof(code_be)),
        net::mutable_buffer(&cr.reason[0], n - 2)}};

    net::buffer_copy(out_bufs, bs);

    cr.code = endian::big_to_native(code_be);
    if(! is_valid_close_code(cr.code))
    {
        // invalid close code
        ec = error::bad_close_code;
        return;
    }

    if(n > 2 && !check_utf8(
        cr.reason.data(), cr.reason.size()))
    {
        // not valid utf-8
        ec = error::bad_close_payload;
        return;
    }
    ec = {};
}

} // detail
} // websocket
} // beast
} // boost

#endif
