//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_STRING_TYPE_HPP
#define BOOST_BEAST_STRING_TYPE_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/core/detail/string_view.hpp>

namespace boost {
namespace beast {

/** (Deprecated: Use `core::string_view` instead.) An alias for `core::string_view`.

    The library uses `boost::core::string_view` directly and no
    longer needs this alias. It is retained for compatibility with
    existing code.
*/
using string_view
    BOOST_BEAST_DEPRECATED("Use core::string_view instead") =
        core::string_view;

/** (Deprecated: Use `core::basic_string_view` instead.) An alias for `core::basic_string_view`.

    The library uses `boost::core::basic_string_view` directly and no
    longer needs this alias. It is retained for compatibility with
    existing code.
*/
template<class CharT>
using basic_string_view
    BOOST_BEAST_DEPRECATED("Use core::basic_string_view instead") =
        core::basic_string_view<CharT>;

template<class S>
inline core::string_view
to_string_view(const S& s)
{
    return core::string_view(s.data(), s.size());
}

} // beast
} // boost

#endif
