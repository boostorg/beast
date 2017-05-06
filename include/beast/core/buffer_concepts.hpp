//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_BUFFER_CONCEPTS_HPP
#define BEAST_BUFFER_CONCEPTS_HPP

#include <beast/config.hpp>
#include <beast/core/detail/buffer_concepts.hpp>
#include <boost/asio/buffer.hpp>
#include <type_traits>

namespace beast {

/// Determine if `T` meets the requirements of @b ConstBufferSequence.
template<class T>
#if BEAST_DOXYGEN
struct is_const_buffer_sequence : std::integral_constant<bool, ...>
#else
struct is_const_buffer_sequence :
    detail::is_buffer_sequence<T,
        boost::asio::const_buffer>::type
#endif
{
};

/// Determine if `T` meets the requirements of @b DynamicBuffer.
template<class T>
#if BEAST_DOXYGEN
struct is_dynamic_buffer : std::integral_constant<bool, ...>
#else
struct is_dynamic_buffer :
    detail::is_dynamic_buffer<T>::type
#endif
{
};

/// Determine if `T` meets the requirements of @b MutableBufferSequence.
template<class T>
#if BEAST_DOXYGEN
struct is_mutable_buffer_sequence : std::integral_constant<bool, ...>
#else
struct is_mutable_buffer_sequence :
    detail::is_buffer_sequence<T,
        boost::asio::mutable_buffer>::type
#endif
{
};

} // beast

#endif
