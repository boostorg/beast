//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_WRITE_OSTREAM_HPP
#define BEAST_WRITE_OSTREAM_HPP

#include <beast/config.hpp>
#include <beast/core/buffer_concepts.hpp>
#include <beast/core/detail/ostream.hpp>
#include <type_traits>
#include <streambuf>
#include <utility>

namespace beast {

/** Return an output stream that formats values into a @b DynamicBuffer.

    This function wraps the caller provided @b DynamicBuffer into
    a `std::ostream` derived class, to allow `operator<<` stream style
    formatting operations.

    @par Example
    @code
        ostream(buffer) << "Hello, world!" << std::endl;
    @endcode

    @note Calling members of the underlying buffer before the output
    stream is destroyed results in undefined behavior.

    @param buffer An object meeting the requirements of @b DynamicBuffer
    into which the formatted output will be placed.

    @return An object derived from `std::ostream` which directs output
    into the specified dynamic buffer. Ownership of the dynamic buffer
    is not transferred. The caller is responsible for ensuring the
    dynamic buffer is not destroyed for the lifetime of the output
    stream.
*/
template<class DynamicBuffer>
#if BEAST_DOXYGEN
implementation_defined
#else
detail::ostream_helper<
    DynamicBuffer, char, std::char_traits<char>,
        std::is_move_constructible<
            detail::ostream_buffer<DynamicBuffer,
                char, std::char_traits<char>>>::value>
#endif
ostream(DynamicBuffer& buffer)
{
    static_assert(is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    return detail::ostream_helper<
        DynamicBuffer, char, std::char_traits<char>,
            std::is_move_constructible<
            detail::ostream_buffer<DynamicBuffer,
                char, std::char_traits<char>>>::value>{buffer};
}

} // beast

#endif
