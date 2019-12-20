//
// Copyright (c) 2018 jackarain (jack dot wgm at gmail dot com)
// Copyright (c) 2019 Richard Hodges (hodges dot r at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef SOCKS_CONFIG_HPP
#define SOCKS_CONFIG_HPP

#include <boost/beast/core/string.hpp>
#include <boost/beast/core/detail/string.hpp>
#include <boost/beast/core/error.hpp>

namespace boost {
namespace asio {
} // asio
} // boost

namespace socks {

using ::boost::beast::string_view;
using error_code = ::boost::beast::error_code;

namespace net = ::boost::beast::net;

} // socks

#endif
