//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_TYPE_TRAITS_HPP
#define BEAST_HTTP_TYPE_TRAITS_HPP

#include <beast/config.hpp>
#include <beast/core/error.hpp>
#include <beast/core/string_view.hpp>
#include <beast/core/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/optional.hpp>
#include <type_traits>
#include <utility>

namespace beast {
namespace http {

template<bool, class, class>
struct message;

namespace detail {

struct fields_model
{
    string_view method() const;
    string_view reason() const;
    string_view target() const;
};

template<class T, class = beast::detail::void_t<>>
struct has_value_type : std::false_type {};

template<class T>
struct has_value_type<T, beast::detail::void_t<
    typename T::value_type
        > > : std::true_type {};

template<class T, class = beast::detail::void_t<>>
struct has_content_length : std::false_type {};

template<class T>
struct has_content_length<T, beast::detail::void_t<decltype(
    std::declval<T>().content_length()
        )> > : std::true_type
{
    static_assert(std::is_convertible<
        decltype(std::declval<T>().content_length()),
            std::uint64_t>::value,
        "Writer::content_length requirements not met");
};

} // detail

/// Determine if `T` meets the requirements of @b Body.
template<class T>
#if BEAST_DOXYGEN
struct is_body : std::integral_constant<bool, ...>{};
#else
using is_body = detail::has_value_type<T>;
#endif

/** Determine if a @b Body type has a reader.

    This metafunction is equivalent to `std::true_type` if:

    @li @b T has a nested type named `reader`

    @li The nested type meets the requirements of @b BodyReader.

    @tparam T The body type to test.
*/
#if BEAST_DOXYGEN
template<class T>
struct is_body_reader : std::integral_constant<bool, ...> {};
#else
template<class T, class = void>
struct is_body_reader : std::false_type {};

template<class T>
struct is_body_reader<T, beast::detail::void_t<
    typename T::reader,
    typename T::reader::const_buffers_type,
        decltype(
    std::declval<typename T::reader&>().init(std::declval<error_code&>()),
    std::declval<boost::optional<std::pair<
            typename T::reader::const_buffers_type, bool>>&>() =
            std::declval<typename T::reader>().get(std::declval<error_code&>()),
        (void)0)>> : std::integral_constant<bool,
    is_const_buffer_sequence<
        typename T::reader::const_buffers_type>::value &&
    std::is_constructible<typename T::reader,
        message<true, T, detail::fields_model> const& >::value
        >
{
};
#endif

/** Determine if a @b Body type has a writer.

    This metafunction is equivalent to `std::true_type` if:

    @li @b T has a nested type named `writer`

    @li The nested type meets the requirements of @b BodyWriter.

    @tparam T The body type to test.
*/
#if BEAST_DOXYGEN
template<class T>
struct is_body_writer : std::integral_constant<bool, ...> {};
#else
template<class T, class = beast::detail::void_t<>>
struct is_body_writer : std::true_type {};

template<class T>
struct is_body_writer<T, beast::detail::void_t<decltype(
    std::declval<typename T::writer::mutable_buffers_type>(),
    std::declval<T::writer>().init(
        std::declval<boost::optional<std::uint64_t>>()),
    std::declval<T::writer>().prepare(
        std::declval<std::size_t>()),
    std::declval<T::writer>().commit(
        std::declval<std::size_t>()),
    std::declval<T::writer>().finish()
            )> > : std::integral_constant<bool,
    is_mutable_buffer_sequence<
        typename T::writer::mutable_buffers_type>::value &&
    std::is_convertible<decltype(
        std::declval<T::writer>().prepare(
            std::declval<std::size_t>())),
        typename T::writer::mutable_buffers_type
            >::value>

{
};
#endif

} // http
} // beast

#endif
