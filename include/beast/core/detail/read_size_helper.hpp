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
#include <boost/throw_exception.hpp>
#include <algorithm>
#include <cstddef>

namespace beast {
namespace detail {

/** Returns a natural read size.

    This function inspects the capacity, size, and maximum
    size of the dynamic buffer. Then it computes a natural
    read size given the passed-in upper limit. It favors
    a read size that does not require a reallocation, subject
    to a reasonable minimum to avoid tiny reads.

    Calls to @ref read_size_helper should be made without
    namespace qualification, i.e. allowing argument dependent
    lookup to take effect, so that overloads of this function
    for specific buffer types may be found.

    @param buffer The dynamic buffer to inspect.

    @param max_size An upper limit on the returned value.
*/
template<class DynamicBuffer>
std::size_t
read_size_helper(
    DynamicBuffer const& buffer, std::size_t max_size)
{
    static_assert(beast::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_ASSERT(max_size >= 1);
    auto const size = buffer.size();
    auto const limit = buffer.max_size() - size;
    BOOST_ASSERT(size <= buffer.max_size());
    return std::min<std::size_t>(
        std::max<std::size_t>(512, buffer.capacity() - size),
        std::min<std::size_t>(max_size, limit));
}

/** Return a non-zero natural read size.
*/
template<class DynamicBuffer>
std::size_t
maybe_read_size_helper(
    DynamicBuffer const& buffer, std::size_t max_size)
{
    static_assert(beast::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    auto const n = read_size_helper(buffer, max_size);
    if(n == 0)
        BOOST_THROW_EXCEPTION(std::length_error{
            "buffer overflow"});
    return n;
}

} // detail
} // beast

#endif
