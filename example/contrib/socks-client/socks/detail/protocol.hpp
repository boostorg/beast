//
// Copyright (c) 2018 jackarain (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef SOCKS_DETAIL_PROTOCOL_HPP
#define SOCKS_DETAIL_PROTOCOL_HPP

#include <socks/config.hpp>

namespace socks {
namespace detail {

enum : char
{
    SOCKS_VERSION_4 = 4,
    SOCKS_VERSION_5 = 5
};

enum : char
{
    SOCKS5_AUTH_NONE = 0x00,
    SOCKS5_AUTH = 0x02,
    SOCKS5_AUTH_UNACCEPTABLE = '\xFF'
};

enum : char
{
    SOCKS_CMD_CONNECT = 0x01,
    SOCKS_CMD_BIND = 0x02,
    SOCKS5_CMD_UDP = 0x03
};

enum : char
{
    SOCKS5_ATYP_IPV4 = 0x01,
    SOCKS5_ATYP_DOMAINNAME = 0x03,
    SOCKS5_ATYP_IPV6 = 0x04
};

enum : char
{
    SOCKS5_SUCCEEDED = 0x00,
    SOCKS5_GENERAL_SOCKS_SERVER_FAILURE,
    SOCKS5_CONNECTION_NOT_ALLOWED_BY_RULESET,
    SOCKS5_NETWORK_UNREACHABLE,
    SOCKS5_CONNECTION_REFUSED,
    SOCKS5_TTL_EXPIRED,
    SOCKS5_COMMAND_NOT_SUPPORTED,
    SOCKS5_ADDRESS_TYPE_NOT_SUPPORTED,
    SOCKS5_UNASSIGNED
};

enum : char
{
    SOCKS4_REQUEST_GRANTED = 90,
    SOCKS4_REQUEST_REJECTED_OR_FAILED,
    SOCKS4_CANNOT_CONNECT_TARGET_SERVER,
    SOCKS4_REQUEST_REJECTED_USER_NO_ALLOW,
};

} // detail
} // socks

#endif
