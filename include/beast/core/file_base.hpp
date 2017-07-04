//
// Copyright (c) 2015-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_CORE_FILE_BASE_HPP
#define BEAST_CORE_FILE_BASE_HPP

#include <beast/core/string.hpp>

namespace beast {

/// The type of file path used by the library
using file_path = string_view;

/** File create and open modes.

    These are used by @ref native_file.
*/
enum class file_mode
{
    /// Open the file for sequential reads
    scan,

    /// Open the file for random reads
    read,

    /// Open the file for random reads and appending writes
    append,

    /// Open the file for random reads and writes
    write
};

} // beast

#endif
