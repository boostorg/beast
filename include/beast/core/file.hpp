//
// Copyright (c) 2015-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_CORE_FILE_HPP
#define BEAST_CORE_FILE_HPP

#include <beast/core/file_base.hpp>
#include <beast/core/file_stdio.hpp>
#include <beast/core/file_win32.hpp>
#include <boost/config.hpp>

namespace beast {

/** An implementation of File.

    This alias is set to the best available implementation
    of @b File given the platform and build settings.
*/
#if defined(BOOST_MSVC) && BEAST_USE_WIN32_FILE
using file = file_win32;

#else
using file = file_stdio;

#endif

} // beast

#endif
