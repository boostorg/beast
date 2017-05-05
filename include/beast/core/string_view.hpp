//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_STRING_VIEW_HPP
#define BEAST_STRING_VIEW_HPP

#include <boost/utility/string_ref.hpp>

namespace beast {

/// The type of string_view used by the library
using string_view = boost::string_ref;

/// The type of basic_string_view used by the library
template<class CharT, class Traits>
using basic_string_view =
    boost::basic_string_ref<CharT, Traits>;

} // beast

#endif
