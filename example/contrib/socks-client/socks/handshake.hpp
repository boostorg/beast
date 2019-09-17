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
#include <string>

namespace socks {

/** Perform the SOCKS v4 handshake in the client role.
*/
template<
    typename AsyncStream,
    typename Handler
>
BOOST_BEAST_ASYNC_RESULT1(Handler)
async_handshake_v4(
    AsyncStream& stream,
    const std::string& hostname,
    unsigned short port,
    std::string const& username,
    Handler&& handler);

/** Perform the SOCKS v5 handshake in the client role.
*/
template<
    typename AsyncStream,
    typename Handler>
BOOST_BEAST_ASYNC_RESULT1(Handler)
async_handshake_v5(
    AsyncStream& stream,
    const std::string& hostname,
    unsigned short port,
    std::string const& username,
    std::string const& password,
    bool use_hostname,
    Handler&& handler);

} // socks

#include <socks/impl/handshake.hpp>

#endif
