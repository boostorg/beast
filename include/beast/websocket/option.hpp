//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_WEBSOCKET_OPTION_HPP
#define BEAST_WEBSOCKET_OPTION_HPP

#include <beast/config.hpp>
#include <beast/websocket/rfc6455.hpp>
#include <beast/core/detail/type_traits.hpp>
#include <boost/throw_exception.hpp>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace beast {
namespace websocket {

namespace detail {

using ping_cb = std::function<void(bool, ping_data const&)>;

} // detail

/** permessage-deflate extension options.

    These settings control the permessage-deflate extension,
    which allows messages to be compressed.

    @note Objects of this type are used with
          @ref beast::websocket::stream::set_option.
*/
struct permessage_deflate
{
    /// `true` to offer the extension in the server role
    bool server_enable = false;

    /// `true` to offer the extension in the client role
    bool client_enable = false;

    /** Maximum server window bits to offer

        @note Due to a bug in ZLib, this value must be greater than 8.
    */
    int server_max_window_bits = 15;

    /** Maximum client window bits to offer

        @note Due to a bug in ZLib, this value must be greater than 8.
    */
    int client_max_window_bits = 15;

    /// `true` if server_no_context_takeover desired
    bool server_no_context_takeover = false;

    /// `true` if client_no_context_takeover desired
    bool client_no_context_takeover = false;

    /// Deflate compression level 0..9
    int compLevel = 8;

    /// Deflate memory level, 1..9
    int memLevel = 4;
};

/** Ping callback option.

    Sets the callback to be invoked whenever a ping or pong is
    received during a call to one of the following functions:

    @li @ref beast::websocket::stream::read
    @li @ref beast::websocket::stream::read_frame
    @li @ref beast::websocket::stream::async_read
    @li @ref beast::websocket::stream::async_read_frame

    Unlike completion handlers, the callback will be invoked
    for each received ping and pong during a call to any
    synchronous or asynchronous read function. The operation is
    passive, with no associated error code, and triggered by reads.

    The signature of the callback must be:
    @code
    void
    callback(
        bool is_pong,               // `true` if this is a pong
        ping_data const& payload    // Payload of the pong frame
    );
    @endcode

    The value of `is_pong` will be `true` if a pong control frame
    is received, and `false` if a ping control frame is received.

    If the read operation receiving a ping or pong frame is an
    asynchronous operation, the callback will be invoked using
    the same method as that used to invoke the final handler.

    @note Objects of this type are used with
          @ref beast::websocket::stream::set_option.
          To remove the ping callback, construct the option with
          no parameters: `set_option(ping_callback{})`
*/
#if BEAST_DOXYGEN
using ping_callback = implementation_defined;
#else
struct ping_callback
{
    detail::ping_cb value;

    ping_callback() = default;
    ping_callback(ping_callback&&) = default;
    ping_callback(ping_callback const&) = default;

    explicit
    ping_callback(detail::ping_cb f)
        : value(std::move(f))
    {
    }
};
#endif


} // websocket
} // beast

#endif
