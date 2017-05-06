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
#include <beast/core/buffer_concepts.hpp>
#include <beast/core/detail/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/optional.hpp>
#include <type_traits>
#include <utility>

namespace beast {
namespace http {

namespace detail {

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

template<class T, class M>
class is_Writer
{
    template<class U, class R = decltype(
        std::declval<U>().init(std::declval<error_code&>()),
            std::true_type{})>
    static R check1(int);
    template<class>
    static std::false_type check1(...);
    using type1 = decltype(check1<T>(0));

    // VFALCO This is unfortunate, we have to provide the template
    //        argument type because this is not a deduced context?
    //
    template<class U, class R =
        std::is_convertible<decltype(
            std::declval<U>().template write<detail::write_function>(
                std::declval<error_code&>(),
                std::declval<detail::write_function>()))
            , bool>>
    static R check2(int);
    template<class>
    static std::false_type check2(...);
    using type2 = decltype(check2<T>(0));

public:
    static_assert(std::is_same<
        typename M::body_type::writer, T>::value,
            "Mismatched writer and message");

    using type = std::integral_constant<bool,
        std::is_nothrow_constructible<T, M const&>::value
        && type1::value
        && type2::value
    >;
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

/** Determine if `T` meets the requirements of @b Writer for `M`.

    @tparam T The type to test.

    @tparam M The message type to test with, which must be of
    type `message`.
*/
template<class T, class M>
#if BEAST_DOXYGEN
struct is_Writer : std::integral_constant<bool, ...> {};
#else
using is_Writer = typename detail::is_Writer<T, M>::type;
#endif

} // http
} // beast

#endif
