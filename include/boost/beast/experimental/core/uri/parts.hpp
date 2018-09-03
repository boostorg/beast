// Copyright (c) 2018 oneiric (oneiric at i2pmail dot org)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_URI_PARTS_HPP
#define BOOST_BEAST_URI_PARTS_HPP

#include <boost/beast/core/string.hpp>

namespace boost {
namespace beast {
namespace uri   {

class parts {
protected:
  std::string scheme_;
  std::string username_;
  std::string password_;
  std::string host_;
  std::string port_;
  std::string path_;
  std::string query_;
  std::string fragment_;

  void reset()
  {
    scheme_.clear();
    username_.clear();
    password_.clear();
    host_.clear();
    port_.clear();
    path_.clear();
    query_.clear();
    fragment_.clear();
  }

public:
  void scheme(std::string part) { scheme_ = part; }
  boost::string_view scheme() const noexcept { return scheme_; }

  void username(std::string part) { username_ = part; }
  boost::string_view username() const noexcept { return username_; }

  void password(std::string part) { password_ = part; }
  boost::string_view password() const noexcept { return password_; }

  boost::string_view user_info() const noexcept {
    return username_ + (password_.empty() ? "" : ":" + password_);
  }

  void host(std::string part) { host_ = part; }
  boost::string_view host() const noexcept { return host_; }

  void port(std::string part) { port_ = part; }
  boost::string_view port() const noexcept { return port_; }

  void path(std::string part) { path_ = part; }
  boost::string_view path() const noexcept { return path_; }

  void query(std::string part) { query_ = part; }
  boost::string_view query() const noexcept { return query_; }

  void fragment(std::string part) { fragment_ = part; }
  boost::string_view fragment() const noexcept { return fragment_; }
};

} // namespace uri
} // namespace beast
} // namespace boost

#endif
