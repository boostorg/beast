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

//------------------------------------------------------------------------------
//
// Buffer concepts
//
//------------------------------------------------------------------------------

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

/// Determine if `T` meets the requirements of @b DynamicBuffer.
#if BEAST_DOXYGEN
template<class T>
struct is_dynamic_buffer : std::integral_constant<bool, ...> {};
#else
template<class T, class = void>
struct is_dynamic_buffer : std::false_type {};

template<class T>
struct is_dynamic_buffer<T, beast::detail::void_t<
        decltype(
    // expressions
    std::declval<std::size_t&>() =
        std::declval<T const&>().size(),
    std::declval<std::size_t&>() =
        std::declval<T const&>().max_size(),
#if 0
    // This check is skipped because boost::asio
    // types are not up to date with net-ts.
    std::declval<std::size_t&>() =
        std::declval<T const&>().capacity(),
#endif
    std::declval<T&>().commit(std::declval<std::size_t>()),
    std::declval<T&>().consume(std::declval<std::size_t>()),
        (void)0)>> : std::integral_constant<bool,
    is_const_buffer_sequence<
        typename T::const_buffers_type>::value &&
    is_mutable_buffer_sequence<
        typename T::mutable_buffers_type>::value &&
    std::is_same<typename T::const_buffers_type,
        decltype(std::declval<T const&>().data())>::value &&
    std::is_same<typename T::mutable_buffers_type,
        decltype(std::declval<T&>().prepare(
            std::declval<std::size_t>()))>::value
        >
{
};
#endif

//------------------------------------------------------------------------------
//
// Handler concepts
//
//------------------------------------------------------------------------------

/// Determine if `T` meets the requirements of @b CompletionHandler.
template<class T, class Signature>
#if BEAST_DOXYGEN
using is_completion_handler = std::integral_constant<bool, ...>;
#else
using is_completion_handler = std::integral_constant<bool,
    std::is_copy_constructible<typename std::decay<T>::type>::value &&
        detail::is_invocable<T, Signature>::value>;
#endif


//------------------------------------------------------------------------------
//
// Stream concepts
//
//------------------------------------------------------------------------------

/// Determine if `T` has the `get_io_service` member.
template<class T>
#if BEAST_DOXYGEN
struct has_get_io_service : std::integral_constant<bool, ...>{};
#else
using has_get_io_service = typename detail::has_get_io_service<T>::type;
#endif

/// Returns `T::lowest_layer_type` if it exists, else `T`
#if BEAST_DOXYGEN
template<class T>
struct get_lowest_layer;
#else
template<class T, class = void>
struct get_lowest_layer
{
    using type = T;
};

template<class T>
struct get_lowest_layer<T, detail::void_t<
    typename T::lowest_layer_type>>
{
    using type = typename T::lowest_layer_type;
};
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

} // beast

#endif
