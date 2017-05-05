//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_DETAIL_STATIC_STRING_HPP
#define BEAST_DETAIL_STATIC_STRING_HPP

#include <beast/core/string_view.hpp>
#include <iterator>
#include <type_traits>

namespace beast {
namespace detail {

// Because k-ballo said so
template<class T>
using is_input_iterator =
    std::integral_constant<bool,
        ! std::is_integral<T>::value>;

template<class CharT, class Traits>
int
lexicographical_compare(
    CharT const* s1, std::size_t n1,
    CharT const* s2, std::size_t n2)
{
    if(n1 < n2)
        return Traits::compare(
            s1, s2, n1) <= 0 ? -1 : 1;
    if(n1 > n2)
        return Traits::compare(
            s1, s2, n2) >= 0 ? 1 : -1;
    return Traits::compare(s1, s2, n1);
}

template<class CharT, class Traits>
inline
int
lexicographical_compare(
    boost::basic_string_ref<CharT, Traits> s1,
    CharT const* s2, std::size_t n2)
{
    return lexicographical_compare<CharT, Traits>(
        s1.data(), s1.size(), s2, n2);
}

template<class CharT, class Traits>
inline
int
lexicographical_compare(
    boost::basic_string_ref<CharT, Traits> s1,
    boost::basic_string_ref<CharT, Traits> s2)
{
    return lexicographical_compare<CharT, Traits>(
        s1.data(), s1.size(), s2.data(), s2.size());
}

} // detail
} // beast

#endif
