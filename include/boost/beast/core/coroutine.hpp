//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_COROUTINE_HPP
#define BOOST_BEAST_COROUTINE_HPP

#include <boost/asio/coroutine.hpp>

namespace boost {
namespace beast {

inline bool is_continuation(asio::coroutine& coroutine) noexcept
{
    asio::detail::coroutine_ref ref{coroutine};
    // We need to reassign the current coroutine state, because
    // coroutine_ref marks the coroutine as complete if it is not modified.
    auto state = (ref = static_cast<int>(ref));
    return state > 0;
}

} // namespace beast
} // namespace boost

#endif // BOOST_BEAST_COROUTINE_HPP
