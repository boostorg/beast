//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_BIND_CONTINUATION_HPP
#define BOOST_BEAST_BIND_CONTINUATION_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/remap_post_to_defer.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/core/empty_value.hpp>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast {

/** Mark a completion handler as a continuation.

    This function wraps a completion handler to associate it with an
    executor whose `post` operation is remapped to the `defer` operation.
    It is used by composed asynchronous operation implementations to
    indicate that a completion handler submitted to an initiating
    function represents a continuation of the current asynchronous
    flow of control.

    @see

    @li <a href="http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4242.html">[N4242] Executors and Asynchronous Operations, Revision 1</a>
*/
template<class Executor, class CompletionHandler>
#if BOOST_BEAST_DOXYGEN
__implementation_defined__
#else
net::executor_binder<typename
    std::decay<CompletionHandler>::type,
    detail::remap_post_to_defer<Executor>>
#endif
bind_continuation(
    Executor const& ex, CompletionHandler&& handler)
{
    return net::bind_executor(
        detail::remap_post_to_defer<Executor>(ex),
        std::forward<CompletionHandler>(handler));
}

} // beast
} // boost

#endif
