//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_CONNECTION_HPP
#define BEAST_HTTP_CONNECTION_HPP

#include <beast/config.hpp>

namespace beast {
namespace http {

/// The type of @ref connection::close
struct close_t {};

/// The type of @ref connection::upgrade
struct upgrade_t {};

/// The type of @ref connection::keep_alive
struct keep_alive_t {};

namespace detail {

template<class = void>
struct connection_impl
{
    static close_t constexpr close{};
    static keep_alive_t constexpr keep_alive{};
    static upgrade_t constexpr upgrade{};
};

template<class _>
constexpr
close_t
connection_impl<_>::close;

template<class _>
constexpr
keep_alive_t
connection_impl<_>::keep_alive;

template<class _>
constexpr
upgrade_t
connection_impl<_>::upgrade;

} // detail

/** HTTP/1 Connection prepare options.

    Each value is an individual settings, they can be combined.
    
    @par Example
    @code
    request<empty_body> req;
    req.version = 11;
    req.method(verb::upgrade);
    req.target("/");
    req.insert("User-Agent", "Beast");
    req.prepare(connection::close, connection::upgrade);
    @endcode

    @note These values are used with @ref message::prepare.
*/
struct connection
#if ! BEAST_DOXYGEN
    : detail::connection_impl<>
#endif
{
#if BEAST_DOXYGEN
    /// Specify connection close semantics.
    static close_t constexpr close;

    /// Specify connection keep-alive semantics.
    static keep_alive_t constexpr keep_alive;

    /// Specify Connection: upgrade.
    static upgrade_t constexpr upgrade;
#endif
};

} // http
} // beast

#endif
