//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_BUFFERS_RANGE_HPP
#define BOOST_BEAST_BUFFERS_RANGE_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/buffers_range.hpp>
#include <boost/asio/buffer.hpp>
#include <functional>

namespace boost {
namespace beast {

/** Return a range representing a const or mutable buffer sequence.

    This function returns an iterable range representing the
    passed buffer sequence. The values obtained when iterating
    the range will always be `const_buffer`, unless the underlying
    buffer sequence is a @em MutableBufferSequence, in which case
    the value obtained when iterating will be a `mutable_buffer`.

    @par Example

    The following function returns the total number of bytes in
    the specified buffer sequence:

    @code
    template<class ConstBufferSequence>
    std::size_t buffer_sequence_size(ConstBufferSequence const& buffers)
    {
        std::size_t size = 0;
        for(auto const buffer : buffers_range(std::ref(buffers)))
            size += buffer.size();
        return size;
    }
    @endcode

    @param buffers The buffer sequence to adapt into a range. The
    range returned from this function will contain a copy of the
    passed buffer sequence. If `buffers` is a `std::reference_wrapper`,
    the range returned from this function will contain a reference to the
    passed buffer sequence. In this case, the caller is responsible
    for ensuring that the lifetime of the buffer sequence is valid for
    the lifetime of the returned range.

    @return An object of unspecified type, meeting the requirements of
    @em ConstBufferSequence, or @em MutableBufferSequence if `buffers`
    is a mutable buffer sequence.
*/
/** @{ */
template<class ConstBufferSequence>
#if BOOST_BEAST_DOXYGEN
__implementation_defined__
#else
detail::buffers_range_adaptor<ConstBufferSequence>
#endif
buffers_range(ConstBufferSequence const& buffers)
{
    static_assert(
        boost::asio::is_const_buffer_sequence<ConstBufferSequence>::value,
        "ConstBufferSequence requirements not met");
    return detail::buffers_range_adaptor<ConstBufferSequence>(buffers);
}

template<class ConstBufferSequence>
#if BOOST_BEAST_DOXYGEN
__implementation_defined__
#else
detail::buffers_range_adaptor<ConstBufferSequence const&>
#endif
buffers_range(std::reference_wrapper<ConstBufferSequence> buffers)
{
    static_assert(
        boost::asio::is_const_buffer_sequence<ConstBufferSequence>::value,
        "ConstBufferSequence requirements not met");
    return detail::buffers_range_adaptor<ConstBufferSequence const&>(buffers.get());
}
/** @} */

} // beast
} // boost

#endif
