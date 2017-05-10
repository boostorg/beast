//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_TYPE_TRAITS_HPP
#define BEAST_TYPE_TRAITS_HPP

#include <beast/config.hpp>
#include <beast/core/detail/type_traits.hpp>
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

/// Determine if `T` has the `get_io_service` member.
template<class T>
#if BEAST_DOXYGEN
struct has_get_io_service : std::integral_constant<bool, ...>{};
#else
using has_get_io_service = typename detail::has_get_io_service<T>::type;
#endif

/// Determine if `T` meets the requirements of @b AsyncReadStream.
template<class T>
#if BEAST_DOXYGEN
struct is_async_read_stream : std::integral_constant<bool, ...>{};
#else
using is_async_read_stream = typename detail::is_async_read_stream<T>::type;
#endif

/// Determine if `T` meets the requirements of @b AsyncWriteStream.
template<class T>
#if BEAST_DOXYGEN
struct is_async_write_stream : std::integral_constant<bool, ...>{};
#else
using is_async_write_stream = typename detail::is_async_write_stream<T>::type;
#endif

/// Determine if `T` meets the requirements of @b SyncReadStream.
template<class T>
#if BEAST_DOXYGEN
struct is_sync_read_stream : std::integral_constant<bool, ...>{};
#else
using is_sync_read_stream = typename detail::is_sync_read_stream<T>::type;
#endif

/// Determine if `T` meets the requirements of @b SyncWriterStream.
template<class T>
#if BEAST_DOXYGEN
struct is_sync_write_stream : std::integral_constant<bool, ...>{};
#else
using is_sync_write_stream = typename detail::is_sync_write_stream<T>::type;
#endif

/// Determine if `T` meets the requirements of @b AsyncStream.
template<class T>
#if BEAST_DOXYGEN
struct is_async_stream : std::integral_constant<bool, ...>{};
#else
using is_async_stream = std::integral_constant<bool,
    is_async_read_stream<T>::value && is_async_write_stream<T>::value>;
#endif

/// Determine if `T` meets the requirements of @b SyncStream.
template<class T>
#if BEAST_DOXYGEN
struct is_sync_stream : std::integral_constant<bool, ...>{};
#else
using is_sync_stream = std::integral_constant<bool,
    is_sync_read_stream<T>::value && is_sync_write_stream<T>::value>;
#endif

/// Determine if `T` meets the requirements of @b CompletionHandler.
template<class T, class Signature>
#if BEAST_DOXYGEN
using is_completion_handler = std::integral_constant<bool, ...>;
#else
using is_completion_handler = std::integral_constant<bool,
    std::is_copy_constructible<typename std::decay<T>::type>::value &&
        detail::is_invocable<T, Signature>::value>;
#endif

} // beast

#endif
