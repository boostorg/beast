//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_DETAIL_READ_SIZE_HELPER_HPP
#define BEAST_DETAIL_READ_SIZE_HELPER_HPP

#include <beast/core/type_traits.hpp>
#include <boost/assert.hpp>
#include <algorithm>
#include <cstddef>

namespace beast {
namespace detail {

template<class DynamicBuffer>
std::size_t
read_size_helper(DynamicBuffer const& buffer, std::size_t max_size)
{
    static_assert(beast::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_ASSERT(max_size >= 1);
    auto const size = buffer.size();
    return std::min<std::size_t>(
        std::max<std::size_t>(512, buffer.capacity() - size),
        std::min<std::size_t>(max_size, buffer.max_size() - size));
}

} // detail
} // beast

#endif
