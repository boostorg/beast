//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DETAIL_BUFFER_HPP
#define BOOST_BEAST_CORE_DETAIL_BUFFER_HPP

#include <boost/beast/core/error.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/optional.hpp>
#include <stdexcept>
#include <algorithm>

namespace boost {
namespace beast {
namespace detail {

template<
    class DynamicBuffer,
    class ErrorValue>
auto
dynamic_buffer_prepare_noexcept(
    DynamicBuffer& buffer,
    std::size_t size,
    error_code& ec,
    ErrorValue ev) ->
        boost::optional<typename
        DynamicBuffer::mutable_buffers_type>
{
    if(buffer.max_size() - buffer.size() < size)
    {
        // length error
        ec = ev;
        return boost::none;
    }
    boost::optional<typename
        DynamicBuffer::mutable_buffers_type> result;
    result.emplace(buffer.prepare(size));
    ec = {};
    return result;
}

template<
    class DynamicBuffer,
    class ErrorValue>
auto
dynamic_buffer_prepare(
    DynamicBuffer& buffer,
    std::size_t size,
    error_code& ec,
    ErrorValue ev) ->
        boost::optional<typename
        DynamicBuffer::mutable_buffers_type>
{
#ifndef BOOST_NO_EXCEPTIONS
    try
    {
        boost::optional<typename
            DynamicBuffer::mutable_buffers_type> result;
        result.emplace(buffer.prepare(size));
        ec = {};
        return result;
    }
    catch(std::length_error const&)
    {
        ec = ev;
    }
    return boost::none;

#else
    return dynamic_buffer_prepare_noexcept(
        buffer, size, ec, ev);
#endif
}

inline
net::mutable_buffer&
truncate(net::mutable_buffer& target, std::size_t n)
{
    n = (std::min)(n, target.size());
    target = net::mutable_buffer(target.data(), n);
    return target;
}

inline
net::const_buffer&
truncate(net::const_buffer& target, std::size_t n)
{
    n = (std::min)(n, target.size());
    target = net::const_buffer(target.data(), n);
    return target;
}

// remove upto n bytes from the front of the buffer and return
// the number of bytes removed
inline
std::size_t
chop_front(
    net::mutable_buffer &target,
    std::size_t n)
{
    auto chopped = (std::min)(target.size(), n);
    target += chopped;
    return chopped;
}

inline
std::size_t
chop_front(
    net::const_buffer &target,
    std::size_t n)
{
    auto chopped = (std::min)(target.size(), n);
    target += chopped;
    return chopped;
}

inline
net::mutable_buffer&
trim(
    net::mutable_buffer& target,
    std::size_t pos,
    std::size_t n)
{
    chop_front(target, pos);
    truncate(target, n);
    return target;
}

inline
net::const_buffer&
trim(
    net::const_buffer& target,
    std::size_t pos,
    std::size_t n)
{
    chop_front(target, pos);
    truncate(target, n);
    return target;
}

inline
net::mutable_buffer
trimmed(
    net::mutable_buffer buf,
    std::size_t pos,
    std::size_t n)
{
    trim(buf, pos, n);
    return buf;
}

inline
net::const_buffer
trimmed(
    net::const_buffer buf,
    std::size_t pos,
    std::size_t n)
{
    trim(buf, pos, n);
    return buf;
}

// Manipulate  a sequence of buffers such that:
// 1. the resulting buffer sequence only represents the bytes from the original
// sequence with the first pos bytes removed and only the subsequent n bytes
// 2. all buffers that are not size 0 will be at the front of the span
// 3. all buffers representing data shall retain relative order
template<class Iter,
    class BufferType =
        typename std::decay<decltype(*std::declval<Iter>())>::type>
auto
trim_buffer_span(
    Iter first,
    Iter last,
    std::size_t pos,
    std::size_t n)
-> typename std::enable_if<
    std::is_same<BufferType, net::mutable_buffer>::value ||
    std::is_same<BufferType, net::const_buffer>::value>::type
{
    while (pos && first != last)
    {
        auto adjust = (std::min)(pos, first->size());
        *first += adjust;
        pos -= adjust;
        if (first->size() == 0)
            std::rotate(first, std::next(first), last--);
    }

    while (n && first != last)
    {
        n-= truncate(*first++, n).size();
    }

    while (first != last)
        truncate(*first++, 0);
}



} // detail
} // beast
} // boost

#endif
