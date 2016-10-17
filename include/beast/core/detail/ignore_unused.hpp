//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_DETAIL_IGNORE_UNUSED_HPP
#define BEAST_DETAIL_IGNORE_UNUSED_HPP

namespace beast {
namespace detail {

template <typename... Ts>
inline void ignore_unused(Ts const& ...)
{}

template <typename... Ts>
inline void ignore_unused()
{}

} // namespace detail
} // namespace boost

#endif
