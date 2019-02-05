//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_STREAM_TRAITS_HPP
#define BOOST_BEAST_STREAM_TRAITS_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/stream_traits.hpp>

namespace boost {
namespace beast {

/** A trait to determine the lowest layer type of a stack of stream layers.

    If `t.next_layer()` is well-defined for an object `t` of type `T`,
    then `lowest_layer_type<T>` will be an alias for
    `lowest_layer_type<decltype(t.next_layer())>`,
    otherwise it will be the type
    `std::remove_reference<T>`.

    @param T The type to determine the lowest layer type of.

    @return The type of the lowest layer.
*/
template<class T>
#if BOOST_BEAST_DOXYGEN
using lowest_layer_type = __see_below__;
#else
using lowest_layer_type = detail::lowest_layer_type<T>;
#endif

/** Return the lowest layer in a stack of stream layers.

    If `t.next_layer()` is well-defined, returns
    `get_lowest_layer(t.next_layer())`. Otherwise, it returns `t`.

    A stream layer is an object of class type which wraps another object through
    composition, and meets some or all of the named requirements of the wrapped
    type while optionally changing behavior. Examples of stream layers include
    `net::ssl::stream` or @ref beast::websocket::stream. The owner of a stream
    layer can interact directly with the wrapper, by passing it to stream
    algorithms. Or, the owner can obtain a reference to the wrapped object by
    calling `next_layer()` and accessing its members. This is necessary when it is
    desired to access functionality in the next layer which is not available
    in the wrapper. For example, @ref websocket::stream permits reading and
    writing, but in order to establish the underlying connection, members
    of the wrapped stream (such as `connect`) must be invoked directly.

    Usually the last object in the chain of composition is the concrete socket
    object (for example, a `net::basic_socket` or a class derived from it).
    The function @ref get_lowest_layer exists to easily obtain the concrete
    socket when it is desired to perform an action that is not prescribed by
    a named requirement, such as changing a socket option, cancelling all
    pending asynchronous I/O, or closing the socket (perhaps by using
    @ref close_socket).

    @par Example
    @code
    // Set non-blocking mode on a stack of stream
    // layers with a regular socket at the lowest layer.
    template <class Stream>
    void set_non_blocking (Stream& stream)
    {
        error_code ec;
        // A compile error here means your lowest layer is not the right type!
        get_lowest_layer(stream).non_blocking(true, ec);
        if(ec)
            throw system_error{ec};
    }
    @endcode

    @param t The layer in a stack of layered objects for which the lowest layer is returned.

    @see @ref close_socket, @ref lowest_layer_type
*/
template<class T>
lowest_layer_type<T>&
get_lowest_layer(T& t) noexcept
{
    return detail::get_lowest_layer_impl(
        t, detail::has_next_layer<T>{});
}

/** A trait to determine the return type of get_executor.

    This type alias will be the type of values returned by
    by calling member `get_exector` on an object of type `T&`.

    @param T The type to query

    @return The type of values returned from `get_executor`.
*/
// Workaround for ICE on gcc 4.8
#if BOOST_BEAST_DOXYGEN
template<class T>
using executor_type = __see_below__;
#elif BOOST_WORKAROUND(BOOST_GCC, < 40900)
template<class T>
using executor_type =
    typename std::decay<T>::type::executor_type;
#else
template<class T>
using executor_type =
    decltype(std::declval<T&>().get_executor());
#endif

} // beast
} // boost

#endif
