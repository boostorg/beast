//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_BUFFER_SIZE_HPP
#define BOOST_BEAST_BUFFER_SIZE_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/static_const.hpp>
#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/type_traits/make_void.hpp>
#include <type_traits>

namespace boost {
namespace beast {

namespace detail {

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

/** Return the total number of bytes in a buffer or buffer sequence

    This function returns the total number of bytes in a buffer,
    buffer sequence, or object convertible to a buffer. Specifically
    it may be passed:

    @li A <em>ConstBufferSequence</em> or <em>MutableBufferSequence</em>

    @li A `net::const_buffer` or `net::mutable_buffer`

    @li An object convertible to `net::const_buffer`

    This function is designed as an easier-to-use replacement for
    `net::buffer_size`. It recognizes customization points found through
    argument-dependent lookup. The call `beast::buffer_size(b)` is
    equivalent to performing:
    @code
    using namespace net;
    buffer_size(b);
    @endcode
    In addition this handles types which are convertible to
    `net::const_buffer`; these are not handled by `net::buffer_size`.

    @note It is expected that a future version of Networking will
    incorporate the features of this function.

    @param buffers The buffer or buffer sequence to calculate the size of.

    @return The total number of bytes in the buffer or sequence.
*/
#if BOOST_BEAST_DOXYGEN
template<class Buffers>
void
buffer_size(Buffers const& buffers);
#else
BOOST_BEAST_INLINE_VARIABLE(buffer_size, detail::buffer_size_impl)
#endif

} // beast
} // boost

#endif
