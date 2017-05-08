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

} // detail
} // http
} // beast

#endif
