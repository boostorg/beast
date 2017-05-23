//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_TEST_BUFFER_TEST_HPP
#define BEAST_TEST_BUFFER_TEST_HPP

#include <beast/core/type_traits.hpp>
#include <beast/core/detail/read_size_helper.hpp>
#include <beast/core/detail/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <algorithm>
#include <type_traits>

namespace beast {
namespace test {

template<class ConstBufferSequence>
typename std::enable_if<
    is_const_buffer_sequence<ConstBufferSequence>::value,
        std::size_t>::type
buffer_count(ConstBufferSequence const& buffers)
{
    return std::distance(buffers.begin(), buffers.end());
}

template<class ConstBufferSequence>
typename std::enable_if<
    is_const_buffer_sequence<ConstBufferSequence>::value,
        std::size_t>::type
size_pre(ConstBufferSequence const& buffers)
{
    std::size_t n = 0;
    for(auto it = buffers.begin(); it != buffers.end(); ++it)
    {
        typename ConstBufferSequence::const_iterator it0(std::move(it));
        typename ConstBufferSequence::const_iterator it1(it0);
        typename ConstBufferSequence::const_iterator it2;
        it2 = it1;
        n += boost::asio::buffer_size(*it2);
        it = std::move(it2);
    }
    return n;
}

template<class ConstBufferSequence>
typename std::enable_if<
    is_const_buffer_sequence<ConstBufferSequence>::value,
        std::size_t>::type
size_post(ConstBufferSequence const& buffers)
{
    std::size_t n = 0;
    for(auto it = buffers.begin(); it != buffers.end(); it++)
        n += boost::asio::buffer_size(*it);
    return n;
}

template<class ConstBufferSequence>
typename std::enable_if<
    is_const_buffer_sequence<ConstBufferSequence>::value,
        std::size_t>::type
size_rev_pre(ConstBufferSequence const& buffers)
{
    std::size_t n = 0;
    for(auto it = buffers.end(); it != buffers.begin();)
        n += boost::asio::buffer_size(*--it);
    return n;
}

template<class ConstBufferSequence>
typename std::enable_if<
    is_const_buffer_sequence<ConstBufferSequence>::value,
        std::size_t>::type
size_rev_post(ConstBufferSequence const& buffers)
{
    std::size_t n = 0;
    for(auto it = buffers.end(); it != buffers.begin();)
    {
        it--;
        n += boost::asio::buffer_size(*it);
    }
    return n;
}

namespace has_read_size_helper
{
    using beast::detail::read_size_helper;

    template<class T, class = void>
    struct trait : std::false_type {};

    template<class T>
    struct trait<T, decltype(
        std::declval<std::size_t&>() =
            read_size_helper(std::declval<T>(), 0),
        (void)0)> : std::true_type
    {
    };
}

// Make sure read_size_helper works
template<class DynamicBuffer>
inline
void
check_read_size_helper()
{
    static_assert(has_read_size_helper::trait<DynamicBuffer>::value,
        "Missing read_size_helper for dynamic buffer");
}

} // test
} // beast

#endif
