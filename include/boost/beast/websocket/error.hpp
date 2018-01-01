//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_ERROR_HPP
#define BOOST_BEAST_WEBSOCKET_ERROR_HPP

#include <boost/beast/websocket/detail/error.hpp>
#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>

namespace boost {
namespace beast {
namespace websocket {

/// Error codes returned from @ref beast::websocket::stream operations.
enum class error
{
    /// Both sides performed a WebSocket close
    closed = 1,

    /// WebSocket connection failed, protocol violation
    failed,

    /// Upgrade handshake failed
    handshake_failed,

    /// buffer overflow
    buffer_overflow,

    /// partial deflate block
    partial_deflate_block
};

/// Error conditions corresponding to sets of error codes.
enum class condition
{
    /** Handshake failed

        This condition indicates that the WebSocket handshake failed. If
        the corresponding HTTP response indicates the keep-alive behavior,
        then the handshake may be reattempted.
    */
    handshake_failed = 1,
 };

} // websocket
} // beast
} // boost

#include <boost/beast/websocket/impl/error.ipp>

#endif
