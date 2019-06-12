//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_DETAIL_STRING_HPP
#define BOOST_BEAST_DETAIL_STRING_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/version.hpp>

#if defined(BOOST_BEAST_USE_STD_STRING_VIEW)
#include <string_view>
#else
#include <boost/utility/string_view.hpp>
#endif

namespace boost {
namespace beast {

namespace detail {

#if defined(BOOST_BEAST_USE_STD_STRING_VIEW)
  using string_view = std::string_view;

  template<class CharT, class Traits>
  using basic_string_view =
      std::basic_string_view<CharT, Traits>;
#else
  using string_view = boost::string_view;

  template<class CharT, class Traits>
  using basic_string_view =
      boost::basic_string_view<CharT, Traits>;
#endif

inline string_view operator "" _sv(char const* p, std::size_t n)
{
    return string_view{p, n};
}

inline
char
ascii_tolower(char c)
{
    return ((static_cast<unsigned>(c) - 65U) < 26) ?
        c + 'a' - 'A' : c;
}
} // detail
} // beast
} // boost

#endif
