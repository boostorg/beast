//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_CONCEPTS_HPP
#define BEAST_HTTP_CONCEPTS_HPP

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

struct write_function
{
    template<class ConstBufferSequence>
    void
    operator()(ConstBufferSequence const&);
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
struct is_Body : std::integral_constant<bool, ...>{};
#else
using is_Body = detail::has_value_type<T>;
#endif

/** Determine if a @b Body has a nested type `reader`.

    @tparam T The type to check, which must meet the
    requirements of @b Body.
*/
#if BEAST_DOXYGEN
template<class T>
struct has_reader : std::integral_constant<bool, ...>{};
#else
template<class T, class = beast::detail::void_t<>>
struct has_reader : std::false_type {};

template<class T>
struct has_reader<T, beast::detail::void_t<
    typename T::reader
        > > : std::true_type {};
#endif

/** Determine if a @b Body has a nested type `writer`.

    @tparam T The type to check, which must meet the
    requirements of @b Body.
*/
#if BEAST_DOXYGEN
template<class T>
struct has_writer : std::integral_constant<bool, ...>{};
#else
template<class T, class = beast::detail::void_t<>>
struct has_writer : std::false_type {};

template<class T>
struct has_writer<T, beast::detail::void_t<
    typename T::writer
        > > : std::true_type {};
#endif

/** Determine if `T` meets the requirements of @b Reader for `M`.

    @tparam T The type to test.

    @tparam M The message type to test with, which must be of
    type `message`.
*/
#if BEAST_DOXYGEN
template<class T, class M>
struct is_Reader : std::integral_constant<bool, ...> {};
#else
template<class T, class M, class = beast::detail::void_t<>>
struct is_Reader : std::true_type {};

template<class T, class M>
struct is_Reader<T, M, beast::detail::void_t<decltype(
    std::declval<typename T::mutable_buffers_type>(),
    std::declval<T>().init(
        std::declval<boost::optional<std::uint64_t>>()),
    std::declval<T>().prepare(
        std::declval<std::size_t>()),
    std::declval<T>().commit(
        std::declval<std::size_t>()),
    std::declval<T>().finish()
            )> > : std::integral_constant<bool,
    is_mutable_buffer_sequence<
        typename T::mutable_buffers_type>::value &&
    std::is_convertible<decltype(
        std::declval<T>().prepare(
            std::declval<std::size_t>())),
        typename T::mutable_buffers_type
            >::value>

{
    static_assert(std::is_same<
        typename M::body_type::reader, T>::value,
            "Mismatched reader and message");
};
#endif

/** Determine if a @b Body type has a writer which meets requirements.

    @tparam T The body type to test.
*/
#if BEAST_DOXYGEN
template<class T>
struct is_Writer : std::integral_constant<bool, ...> {};
#else
template<class T, class = void>
struct is_Writer : std::false_type {};

template<class T>
struct is_Writer<T, beast::detail::void_t<
    typename T::writer,
    typename T::writer::const_buffers_type,
        decltype(
    std::declval<typename T::writer&>().init(std::declval<error_code&>()),
    std::declval<boost::optional<std::pair<
            typename T::writer::const_buffers_type, bool>>&>() =
            std::declval<typename T::writer>().get(std::declval<error_code&>()),
        (void)0)>> : std::integral_constant<bool,
    is_const_buffer_sequence<
        typename T::writer::const_buffers_type>::value &&
    std::is_constructible<typename T::writer,
        message<true, T, detail::fields_model> const& >::value
        >
{
};
#endif

} // http
} // beast

#endif
