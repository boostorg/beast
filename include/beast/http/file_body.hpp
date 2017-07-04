//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_FILE_BODY_HPP
#define BEAST_HTTP_FILE_BODY_HPP

#include <boost/config.hpp>

#include <beast/http/file_body_linux.hpp>
#include <beast/http/file_body_win32.hpp>

#if BEAST_NO_WIN32_FILE
# include <beast/http/file_body_stdc.hpp>
#endif

namespace beast {
namespace http {

#if ! BEAST_NO_LINUX_FILE
using file_body = file_body_linux;

#elif ! BEAST_NO_WIN32_FILE
using file_body = file_body_win32;

#else
using file_body = file_body_stdc;

#endif

} // http
} // beast

#endif
