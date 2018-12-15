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
#include <boost/mp11/function.hpp>
#include <type_traits>

namespace boost {
namespace beast {

/** Determine if a list of types satisfy the <em>ConstBufferSequence</em> requirements.

    This metafunction is used to determine if all of the specified types
    meet the requirements for const buffer sequences. This type alias
    will be `std::true_type` if each specified type meets the requirements,
    otherwise, this type alias will be `std::false_type`.

    @tparam TN A list of zero or more types to check. If this list is
    empty, the resulting type alias will be `std::true_type`.
*/
#if BOOST_BEAST_DOXYGEN
template<class... TN>
struct is_const_buffer_sequence : __see_below__ {};
//using is_const_buffer_sequence = __see_below__;
#else
template<class... TN>
using is_const_buffer_sequence = mp11::mp_all<
    net::is_const_buffer_sequence<TN>...>;
#endif

/** Determine if a list of types satisfy the <em>MutableBufferSequence</em> requirements.

    This metafunction is used to determine if all of the specified types
    meet the requirements for mutable buffer sequences. This type alias
    will be `std::true_type` if each specified type meets the requirements,
    otherwise, this type alias will be `std::false_type`.

    @tparam TN A list of zero or more types to check. If this list is
    empty, the resulting type alias will be `std::true_type`.
*/
#if BOOST_BEAST_DOXYGEN
template<class... TN>
struct is_mutable_buffer_sequence : __see_below__ {};
//using is_mutable_buffer_sequence = __see_below__;
#else
template<class... TN>
using is_mutable_buffer_sequence = mp11::mp_all<
    net::is_mutable_buffer_sequence<TN>...>;
#endif

/** Type alias for the underlying buffer type of a list of buffer sequence types.

    This metafunction is used to determine the underlying buffer type for
    a list of buffer sequence. The equivalent type of the alias will vary
    depending on the template type argument:

    @li If every type in the list is a <em>MutableBufferSequence</em>,
        the resulting type alias will be `net::mutable_buffer`, otherwise

    @li The resulting type alias will be `net::const_buffer`.

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

    @tparam TN A list of zero or more types to check. If this list is
    empty, the resulting type alias will be `net::mutable_buffer`.
*/
template<class... TN>
#if BOOST_BEAST_DOXYGEN
struct buffers_type : __see_below__ {};
//using buffers_type = __see_below__;
#else
using buffers_type = typename
    std::conditional<
        is_mutable_buffer_sequence<TN...>::value,
        net::mutable_buffer,
        net::const_buffer>::type;
#endif

} // beast
} // boost

#endif
