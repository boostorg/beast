//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_BUFFER_TRAITS_HPP
#define BOOST_BEAST_BUFFER_TRAITS_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/buffer.hpp>
#include <type_traits>

namespace boost {
namespace beast {

/** Type alias used to obtain the underlying buffer type of a buffer sequence.

    This metafunction is used to determine the underlying buffer type for
    a given buffer sequence. The equivalent type of the alias will vary
    depending on the template type argument:

    @li If the template argument is a <em>MutableBufferSequence</em>,
        the resulting type alias will be `net::mutable_buffer`, else

    @li If the template argument is a <em>ConstBufferSequence</em>,
        the resulting type alias will be `net::const_buffer`, otherwise

    @li The resulting type alias will be `void`.

    @par Example
    The following code returns the first buffer in a buffer sequence,
    or generates a compilation error if the argument is not a buffer
    sequence:
    @code
    template <class BufferSequence>
    buffers_type <BufferSequence>
    buffers_front (BufferSequence const& buffers)
    {
        static_assert(
            net::is_const_buffer_sequence<BufferSequence>::value,
            "BufferSequence requirements not met");
        auto const first = net::buffer_sequence_begin (buffers);
        if (first == net::buffer_sequence_end (buffers))
            return {};
        return *first;
    }
    @endcode

    @param T The buffer sequence type to use.
*/
template<class T>
#if BOOST_BEAST_DOXYGEN
struct buffers_type : __see_below__ {};
//using buffers_type = __see_below__;
#else
using buffers_type = typename
    std::conditional<
        net::is_mutable_buffer_sequence<T>::value,
        net::mutable_buffer,
        typename std::conditional<
            net::is_const_buffer_sequence<T>::value,
            net::const_buffer,
            void>::type>::type;
#endif

} // beast
} // boost

#endif
