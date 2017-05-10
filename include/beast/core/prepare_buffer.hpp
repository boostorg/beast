//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_PREPARE_BUFFER_HPP
#define BEAST_PREPARE_BUFFER_HPP

#include <beast/config.hpp>
#include <beast/core/detail/prepare_buffer.hpp>
#include <boost/asio/buffer.hpp>
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace beast {

/** Return a shortened buffer.

    The returned buffer points to the same memory as the
    passed buffer, but with a size that is equal to or less
    than the size of the original buffer.

    @param n The size of the returned buffer.

    @param buffer The buffer to shorten. Ownership of the
    underlying memory is not transferred.

    @return A new buffer that points to the first `n` bytes
    of the original buffer.
*/
inline
boost::asio::const_buffer
prepare_buffer(std::size_t n,
    boost::asio::const_buffer buffer)
{
    using boost::asio::buffer_cast;
    using boost::asio::buffer_size;
    return { buffer_cast<void const*>(buffer),
        (std::min)(n, buffer_size(buffer)) };
}

/** Return a shortened buffer.

    The returned buffer points to the same memory as the
    passed buffer, but with a size that is equal to or less
    than the size of the original buffer.

    @param n The size of the returned buffer.

    @param buffer The buffer to shorten. Ownership of the
    underlying memory is not transferred.

    @return A new buffer that points to the first `n` bytes
    of the original buffer.
*/
inline
boost::asio::mutable_buffer
prepare_buffer(std::size_t n,
    boost::asio::mutable_buffer buffer)
{
    using boost::asio::buffer_cast;
    using boost::asio::buffer_size;
    return { buffer_cast<void*>(buffer),
        (std::min)(n, buffer_size(buffer)) };
}

/** Return a shortened buffer sequence.

    This function returns a new buffer sequence which adapts the
    passed buffer sequence and efficiently presents a shorter subset
    of the original list of buffers starting with the first byte of
    the original sequence.

    @param n The maximum number of bytes in the wrapped
    sequence. If this is larger than the size of passed,
    buffers, the resulting sequence will represent the
    entire input sequence.

    @param buffers The buffer sequence to adapt. A copy of
    the sequence will be made, but ownership of the underlying
    memory is not transferred.
*/
template<class BufferSequence>
#if BEAST_DOXYGEN
implementation_defined
#else
inline
detail::prepare_buffers_helper<BufferSequence>
#endif
prepare_buffers(std::size_t n, BufferSequence const& buffers)
{
    return detail::prepare_buffers_helper<BufferSequence>(n, buffers);
}

} // beast

#endif
