//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CLOSE_SOCKET_HPP
#define BOOST_BEAST_CLOSE_SOCKET_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/basic_socket.hpp>

namespace boost {
namespace beast {

namespace detail {

template<class Protocol>
void
beast_close_socket(
    net::basic_socket<Protocol>& sock)
{
    boost::system::error_code ec;
    sock.close(ec);
}

} // detail

/** Close a socket.

    @param sock The socket to close.
*/
template<class Socket>
void
close_socket(Socket& sock)
{
    using detail::beast_close_socket;
    beast_close_socket(sock);
}

} // beast
} // boost

#endif
