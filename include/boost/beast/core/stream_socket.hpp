//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_STREAM_SOCKET_HPP
#define BOOST_BEAST_CORE_STREAM_SOCKET_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/basic_stream_socket.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace boost {
namespace beast {

/** A TCP/IP stream socket which supports timeouts and rate limits
*/
using stream_socket = basic_stream_socket<
    net::ip::tcp,
    net::io_context::executor_type>;

} // beast
} // boost

#endif
