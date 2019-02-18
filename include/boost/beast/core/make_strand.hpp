//
// Copyright (c) 2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_MAKE_STRAND_HPP
#define BOOST_BEAST_MAKE_STRAND_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/asio/is_executor.hpp>
#include <boost/asio/strand.hpp>
#include <type_traits>

namespace boost {
namespace beast {

/** Return a strand usable with the specified execution context.
*/
#if BOOST_BEAST_DOXYGEN
template<class ExecutionContext>
__see_below__
#else
template<class ExecutionContext
    ,class = typename std::enable_if<
        has_get_executor<ExecutionContext>::value &&
        std::is_convertible<
            ExecutionContext&,
            net::execution_context&>::value>::type
>
net::strand<executor_type<ExecutionContext>>
#endif
make_strand(ExecutionContext& context)
{
    return net::strand<executor_type<
        ExecutionContext>>{context.get_executor()};
}

/** Return a strand usable with the specified executor.
*/
template<class Executor
#if ! BOOST_BEAST_DOXYGEN
    , class = typename
        net::is_executor<Executor>::type
#endif
>
net::strand<Executor>
make_strand(Executor const& ex)
{
    return net::strand<Executor>{ex};
}

} // beast
} // boost

#endif
