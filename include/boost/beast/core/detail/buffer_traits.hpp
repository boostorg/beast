//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_DETAIL_BUFFER_TRAITS_HPP
#define BOOST_BEAST_DETAIL_BUFFER_TRAITS_HPP

#include <boost/asio/buffer.hpp>
#include <boost/config/workaround.hpp>
#include <boost/type_traits/make_void.hpp>
#include <cstdint>
#include <type_traits>

namespace boost {
namespace beast {
namespace detail {

#if BOOST_WORKAROUND(BOOST_MSVC, < 1910)

template<class T>
struct buffers_iterator_type_helper
{
    using type = decltype(
        net::buffer_sequence_begin(
            std::declval<T const&>()));
};

template<>
struct buffers_iterator_type_helper<
    net::const_buffer>
{
    using type = net::const_buffer const*;
};

template<>
struct buffers_iterator_type_helper<
    net::mutable_buffer>
{
    using type = net::mutable_buffer const*;
};

#endif

template<class T, class = void>
struct has_buffer_size_impl : std::false_type
{
};

template<class T>
struct has_buffer_size_impl<T, boost::void_t<
    decltype(std::declval<std::size_t&>() =
        std::declval<T const&>().buffer_size_impl())>>
    : std::true_type
{
};

struct buffer_size_impl
{
    template<
        class B,
        class = typename std::enable_if<
            std::is_convertible<
                B, net::const_buffer>::value>::type>
    std::size_t
    operator()(B const& b) const
    {
        return net::const_buffer(b).size();
    }

    template<
        class B,
        class = typename std::enable_if<
            ! std::is_convertible<
                B, net::const_buffer>::value>::type,
        class = typename std::enable_if<
            net::is_const_buffer_sequence<B>::value &&
            ! has_buffer_size_impl<B>::value>::type>
    std::size_t
    operator()(B const& b) const
    {
        using net::buffer_size;
        return buffer_size(b);
    }

    template<
        class B,
        class = typename std::enable_if<
            ! std::is_convertible<B, net::const_buffer>::value>::type,
        class = typename std::enable_if<
            net::is_const_buffer_sequence<B>::value>::type,
        class = typename std::enable_if<
            has_buffer_size_impl<B>::value>::type>
    std::size_t
    operator()(B const& b) const
    {
        return b.buffer_size_impl();
    }
};

/** Return `true` if a buffer sequence is empty

    This is sometimes faster than using @ref buffer_size
*/
template<class ConstBufferSequence>
bool
buffers_empty(ConstBufferSequence const& buffers)
{
    auto it = net::buffer_sequence_begin(buffers);
    auto end = net::buffer_sequence_end(buffers);
    while(it != end)
    {
        if(net::const_buffer(*it).size() > 0)
            return false;
        ++it;
    }
    return true;
}

} // detail
} // beast
} // boost

#endif
