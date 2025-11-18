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

template<class S>
inline boost::core::string_view
to_string_view(const S& s)
{
    return boost::core::string_view(s.data(), s.size());
}

} // beast
} // boost

#endif
