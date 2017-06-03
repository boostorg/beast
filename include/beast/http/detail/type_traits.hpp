//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_DETAIL_TYPE_TRAITS_HPP
#define BEAST_HTTP_DETAIL_TYPE_TRAITS_HPP

#include <beast/core/detail/type_traits.hpp>

namespace beast {
namespace http {

template<bool isRequest, class Fields>
struct header;

template<bool, class, class>
struct message;

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
} // http
} // beast

#endif
