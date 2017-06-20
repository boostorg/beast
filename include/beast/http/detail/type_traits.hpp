//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_DETAIL_TYPE_TRAITS_HPP
#define BEAST_HTTP_DETAIL_TYPE_TRAITS_HPP

#include <beast/core/detail/type_traits.hpp>
#include <boost/optional.hpp>
#include <cstdint>

namespace beast {
namespace http {

template<bool isRequest, class Fields>
struct header;

template<bool, class, class>
struct message;

template<bool isRequest,class Body, class Fields>
class parser;

namespace detail {

template<class T>
class is_header_impl
{
    template<bool b, class F>
    static std::true_type check(
        header<b, F> const*);
    static std::false_type check(...);
public:
    using type = decltype(check((T*)0));
};

template<class T>
using is_header = typename is_header_impl<T>::type;

template<class T>
struct is_parser : std::false_type {};

template<bool isRequest, class Body, class Fields>
struct is_parser<parser<isRequest, Body, Fields>> : std::true_type {};

struct fields_model
{
    string_view method() const;
    string_view reason() const;
    string_view target() const;

protected:
    void set_method_impl(string_view s);
    void set_target_impl(string_view s);
    void set_reason_impl(string_view s);
    string_view get_method_impl() const;
    string_view get_target_impl() const;
    string_view get_reason_impl() const;
    void prepare_payload_impl(bool b, boost::optional<std::uint64_t> n);
};

template<class T, class = beast::detail::void_t<>>
struct has_value_type : std::false_type {};

template<class T>
struct has_value_type<T, beast::detail::void_t<
    typename T::value_type
        > > : std::true_type {};

/** Determine if a @b Body type has a size

    This metafunction is equivalent to `std::true_type` if
    Body contains a static member function called `size`.
*/
template<class T, class = void>
struct is_body_sized : std::false_type {};

template<class T>
struct is_body_sized<T, beast::detail::void_t<
    typename T::value_type,
        decltype(
    std::declval<std::uint64_t&>() =
        T::size(std::declval<typename T::value_type const&>()),
    (void)0)>> : std::true_type {};

template<class T>
struct is_fields_helper : T
{
    template<class U = is_fields_helper>
    static auto f1(int) -> decltype(
        void(std::declval<U&>().set_method_impl(std::declval<string_view>())),
        std::true_type());
    static auto f1(...) -> std::false_type;
    using t1 = decltype(f1(0));

    template<class U = is_fields_helper>
    static auto f2(int) -> decltype(
        void(std::declval<U&>().set_target_impl(std::declval<string_view>())),
        std::true_type());
    static auto f2(...) -> std::false_type;
    using t2 = decltype(f2(0));

    template<class U = is_fields_helper>
    static auto f3(int) -> decltype(
        void(std::declval<U&>().set_reason_impl(std::declval<string_view>())),
        std::true_type());
    static auto f3(...) -> std::false_type;
    using t3 = decltype(f3(0));

    template<class U = is_fields_helper>
    static auto f4(int) -> decltype(
        std::declval<string_view&>() = std::declval<U&>().get_method_impl(),
        std::true_type());
    static auto f4(...) -> std::false_type;
    using t4 = decltype(f4(0));

    template<class U = is_fields_helper>
    static auto f5(int) -> decltype(
        std::declval<string_view&>() = std::declval<U&>().get_target_impl(),
        std::true_type());
    static auto f5(...) -> std::false_type;
    using t5 = decltype(f5(0));

    template<class U = is_fields_helper>
    static auto f6(int) -> decltype(
        std::declval<string_view&>() = std::declval<U&>().get_reason_impl(),
        std::true_type());
    static auto f6(...) -> std::false_type;
    using t6 = decltype(f6(0));

    template<class U = is_fields_helper>
    static auto f7(int) -> decltype(
        void(std::declval<U&>().prepare_payload_impl(
            std::declval<bool>(),
            std::declval<boost::optional<std::uint64_t>>())),
        std::true_type());
    static auto f7(...) -> std::false_type;
    using t7 = decltype(f7(0));

    using type = std::integral_constant<bool,
        t1::value && t2::value && t3::value &&
        t4::value && t5::value && t6::value &&
        t7::value
    >;
};

} // detail
} // http
} // beast

#endif
