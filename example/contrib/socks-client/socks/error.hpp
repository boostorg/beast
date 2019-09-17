//
// Copyright (c) 2018 jackarain (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef SOCKS_ERROR_HPP
#define SOCKS_ERROR_HPP

#include <socks/config.hpp>
#include <boost/beast/core/error.hpp>

namespace socks {

using ::boost::beast::error_code;
using ::boost::beast::system_error;
using ::boost::beast::error_category;
using ::boost::beast::error_condition;

enum class error
{
    /// SOCKS unsupported version.
    socks_unsupported_version = 1000,

    /// SOCKS username required.
    socks_username_required,

    /// SOCKS unsupported authentication version.
    socks_unsupported_authentication_version,

    /// SOCKS authentication error.
    socks_authentication_error,

    /// SOCKS general failure.
    socks_general_failure,

    /// connection not allowed by ruleset.
    socks_connection_not_allowed_by_ruleset,

    /// Network unreachable.
    socks_network_unreachable,

    /// Host unreachable.
    socks_host_unreachable,

    /// Connection refused.
    socks_connection_refused,

    /// TTL expired.
    socks_ttl_expired,

    /// SOCKS command not supported.
    socks_command_not_supported,

    /// Address type not supported.
    socks_address_type_not_supported,

    /// unassigned.
    socks_unassigned,

    /// unknown error.
    socks_unknown_error,

    /// SOCKS no identd running.
    socks_no_identd,

    /// SOCKS no identd running.
    socks_identd_error,

    /// request rejected or failed.
    socks_request_rejected_or_failed,

    /// request rejected becasue SOCKS server cannot connect to identd on the client.
    socks_request_rejected_cannot_connect,

    /// request rejected because the client program and identd report different user - ids
    socks_request_rejected_incorrect_userid,
};

} // socks

#include <socks/impl/error.hpp>
#include <socks/impl/error.ipp>

#endif
