#ifndef BOOST_BEAST_EXAMPLES_SOCKS_USERNAME_PASSWORD_AUTHENTICATION_HPP
#define BOOST_BEAST_EXAMPLES_SOCKS_USERNAME_PASSWORD_AUTHENTICATION_HPP

#include <socks/config.hpp>
#include <socks/error.hpp>
#include <boost/asio/async_result.hpp>
#include <memory>
#include <string_view>
#include "impl/socks5_username_password_authentication.hpp"

namespace socks
{
  template<class Stream, class Handler>
  auto async_socks5_auth_username_password(
    Stream& stream, 
    string_view username,
    string_view password,
    Handler&& handler)
  -> BOOST_BEAST_ASYNC_RESULT1(Handler);
}

#include "impl/socks5_username_password_authentication.hpp"

#endif
