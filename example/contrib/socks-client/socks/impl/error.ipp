//
// Copyright (c) 2018 jackarain (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef SOCKS_IMPL_ERROR_IPP
#define SOCKS_IMPL_ERROR_IPP

#include <socks/error.hpp>

namespace socks {

namespace detail {


} // detail

error_code
make_error_code(error e)
{
  static struct : error_category
  {
  public:
    const char*
    name() const noexcept override
    {
      return "SOCKS";
    }

    std::string
    message(int ev) const override
    {
      switch(static_cast<error>(ev))
      {
      case error::response_unrecognised_version:
          return "SOCKS RESPONSE unrecognised version";
        case error::socks_not_implemented:
          return "SOCKS feature not implemented";
      case error::socks_unsupported_version:
          return "SOCKS unsupported version";
      case error::socks_username_required:
          return "SOCKS username required";
      case error::socks_unsupported_authentication_version:
          return "SOCKS unsupported authentication version";
      case error::socks_authentication_error:
          return "SOCKS authentication error";
      case error::socks_general_failure:
          return "SOCKS general failure";
      case error::socks_connection_not_allowed_by_ruleset:
          return "SOCKS connection not allowed by ruleset";
      case error::socks_network_unreachable:
          return "SOCKS network unreachable";
      case error::socks_host_unreachable:
          return "SOCKS host unreachable";
      case error::socks_connection_refused:
          return "SOCKS connection refused";
      case error::socks_ttl_expired:
          return "SOCKS TTL expired";
      case error::socks_command_not_supported:
          return "SOCKS command not supported";
      case error::socks_address_type_not_supported:
          return "SOCKS Address type not supported";
      case error::socks_unassigned:
          return "SOCKS unassigned";
      case error::socks_unknown_error:
          return "SOCKS unknown error";
      case error::socks_no_identd:
          return "SOCKS no identd running";
      case error::socks_identd_error:
          return "SOCKS no identd running";
      case error::socks_request_rejected_or_failed:
          return "SOCKS request rejected or failed";
      case error::socks_request_rejected_cannot_connect:
          return "SOCKS request rejected becasue SOCKS server cannot connect to identd on the client";
      case error::socks_request_rejected_incorrect_userid:
          return "SOCKS request rejected because the client program and identd report different user";
      default:
          return "SOCKS Unknown PROXY error";
      }
    }

    error_condition
    default_error_condition(int ev) const noexcept override
    {
        return error_condition(ev, *this);
    }

  } const category;

  auto code = std::underlying_type<error>::type(e);

  return error_code(code, category);
}

} // socks

#endif
