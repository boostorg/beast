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
#include <boost/beast/core/detail/static_const.hpp>
#include <boost/asio/basic_socket.hpp>

namespace boost {
namespace beast {

/** Default socket close function.

    This function is not meant to be called directly. Instead, it
    is called automatically when using @ref close_socket. To enable
    closure of user-defined types or classes derived from a particular
    user-defined type, this function should be overloaded in the
    corresponding namespace for the type in question.

    @see close_socket
*/
template<class Protocol BOOST_ASIO_SVC_TPARAM>
void
beast_close_socket(
    net::basic_socket<Protocol BOOST_ASIO_SVC_TPARAM>& sock)
{
    boost::system::error_code ec;
    sock.close(ec);
}

namespace detail {

struct close_socket_impl
{
    template<class T>
    void
    operator()(T& t) const
    {
        using beast::beast_close_socket;
        beast_close_socket(t);
    }
};

} // detail

/** Close a socket or socket-like object.

    This function attempts to close an object representing a socket.
    In this context, a socket is an object for which an unqualified
    call to the function `void beast_close_socket(Socket&)` is
    well-defined. The function `beast_close_socket` is a
    <em>customization point</em>, allowing user-defined types to
    provide an algorithm for performing the close operation by
    overloading this function for the type in question.

    Since the customization point is a function call, the normal
    rules for finding the correct overload are applied including
    the rules for argument-dependent lookup ("ADL"). This permits
    classes derived from a type for which a customization is provided
    to inherit the customization point.

    An overload for the networking class template `net::basic_socket`
    is provided, which implements the close algorithm for all socket-like
    objects (hence the name of this customization point). When used
    in conjunction with @ref get_lowest_layer, a generic algorithm
    operating on a layered stream can perform a closure of the underlying
    socket without knowing the exact list of concrete types.

    @par Example 1
    The following generic function synchronously sends a message
    on the stream, then closes the socket.
    @code
    template <class WriteStream>
    void hello_and_close (WriteStream& stream)
    {
        net::write(stream, net::const_buffer("Hello, world!", 13));
        close_socket(get_lowest_layer(stream));
    }
    @endcode

    To enable closure of user defined types, it is necessary to provide
    an overload of the function `beast_close_socket` for the type.

    @par Example 2
    The following code declares a user-defined type which contains a
    private socket, and provides an overload of the customization
    point which closes the private socket.
    @code
    class my_socket
    {
        net::ip::tcp::socket sock_;

    public:
        my_socket(net::io_context& ioc)
            : sock_(ioc)
        {
        }

        friend void beast_close_socket(my_socket& s)
        {
            error_code ec;
            s.sock_.close(ec);
            // ignore the error
        }
    };
    @endcode

    @param sock The socket to close. If the customization point is not
    defined for the type of this object, or one of its base classes,
    then a compiler error results.

    @see beast_close_socket
*/
#if BOOST_BEAST_DOXYGEN
template<class Socket>
void
close_socket(Socket& sock);
#else
BOOST_BEAST_INLINE_VARIABLE(close_socket, detail::close_socket_impl)
#endif

} // beast
} // boost

#endif
