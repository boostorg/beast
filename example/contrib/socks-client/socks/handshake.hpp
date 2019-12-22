//
// Copyright (c) 2018 jackarain (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef SOCKS_HANDSHAKE_HPP
#define SOCKS_HANDSHAKE_HPP

#include <socks/config.hpp>
#include <socks/error.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/utility/string_view.hpp>

namespace socks {

/** Perform the SOCKS v4 handshake in the client role.
 * @param stream is a reference to the stream object
 * @param hostanme is a string_view representing the target host. It may be
 *                 dotted decimal ip4 or a fqdn. If an fqdn is provided, it will
 *                 be resolved during this async operation
 * @param service is a string_view containing the port or service name of the
 *                intended remote connection
 * @param username is a string_view containing the username for socks4
 *                 authentication
 * @param Handler is a CompletionHandler which will be be invoked exactly once
 *                as if by @see post
*/
template<
    typename AsyncStream,
    typename Handler
>
BOOST_BEAST_ASYNC_RESULT1(Handler)
async_handshake_v4(
    AsyncStream& stream,
    string_view hostname,
    string_view service,
    string_view username,
    Handler&& handler);

} // socks

#include <socks/impl/async_handshake_v4.hpp>

#endif
