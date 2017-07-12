//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_FILE_BODY_HPP
#define BEAST_HTTP_FILE_BODY_HPP

#include <beast/core/file.hpp>
#include <beast/http/basic_file_body.hpp>
#include <boost/assert.hpp>
#include <boost/optional.hpp>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <utility>

namespace beast {
namespace http {

/// A message body represented by a file on the filesystem.
using file_body = basic_file_body<file>;

} // http
} // beast

#include <beast/http/impl/file_body_win32.ipp>

#endif
