//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_BUFFERS_PREFIX_HPP
#define BOOST_BEAST_BUFFERS_PREFIX_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/optional/optional.hpp> // for in_place_init_t
#include <algorithm>
#include <cstdint>
#include <type_traits>

namespace boost {
namespace beast {

/** A buffer sequence adapter that shortens the sequence size.

    The class adapts a buffer sequence to efficiently represent
    a shorter subset of the original list of buffers starting
    with the first byte of the original sequence.

    @tparam BufferSequence The buffer sequence to adapt.
*/
template<class BufferSequence>
class buffers_prefix_view
{
    using buffers_type = typename
        std::decay<BufferSequence>::type;

    using iter_type = typename
        detail::buffer_sequence_iterator<buffers_type>::type;

    BufferSequence bs_;
    std::size_t size_;
    std::size_t remain_;
    iter_type end_;

    template<class Deduced>
    buffers_prefix_view(
            Deduced&& other, std::size_t dist)
        : bs_(std::forward<Deduced>(other).bs_)
        , size_(other.size_)
        , remain_(other.remain_)
        , end_(std::next(bs_.begin(), dist))
    {
    }

    void
    setup(std::size_t size);

public:
    /// The type for each element in the list of buffers.
    using value_type = typename std::conditional<
        std::is_convertible<typename
            std::iterator_traits<iter_type>::value_type,
                net::mutable_buffer>::value,
                    net::mutable_buffer,
                        net::const_buffer>::type;

#if BOOST_BEAST_DOXYGEN
    /// A bidirectional iterator type that may be used to read elements.
    using const_iterator = __implementation_defined__;

#else
    class const_iterator;

#endif

    /// Move Constructor
    buffers_prefix_view(buffers_prefix_view&&);

    /// Copy Constructor
    buffers_prefix_view(buffers_prefix_view const&);

    /// Move Assignment
    buffers_prefix_view& operator=(buffers_prefix_view&&);

    /// Copy Assignment
    buffers_prefix_view& operator=(buffers_prefix_view const&);

    /** Construct a buffer sequence prefix.

        @param size The maximum number of bytes in the prefix.
        If this is larger than the size of passed, buffers,
        the resulting sequence will represent the entire
        input sequence.

        @param buffers The buffer sequence to adapt. A copy of
        the sequence will be made, but ownership of the underlying
        memory is not transferred. The copy is maintained for
        the lifetime of the view.
    */
    buffers_prefix_view(
        std::size_t size,
        BufferSequence const& buffers);

    /** Construct a buffer sequence prefix in-place.

        @param size The maximum number of bytes in the prefix.
        If this is larger than the size of passed, buffers,
        the resulting sequence will represent the entire
        input sequence.

        @param args Arguments forwarded to the contained buffers constructor.
    */
    template<class... Args>
    buffers_prefix_view(
        std::size_t size,
        boost::in_place_init_t,
        Args&&... args);

    /// Returns an iterator to the first buffer in the sequence
    const_iterator
    begin() const;

    /// Returns an iterator to one past the last buffer in the sequence 
    const_iterator
    end() const;
};

//------------------------------------------------------------------------------

/** Returns a prefix of a constant buffer.

    The returned buffer points to the same memory as the passed
    buffer, but with a size that is equal to or smaller.

    @param size The maximum size of the returned buffer in bytes. If
    this is greater than or equal to the size of the passed buffer,
    the result will have the same size as the original buffer.

    @param buffer The buffer to return a prefix for. The
    underlying memory is not modified, and ownership of the
    memory is not transferred.

    @return A constant buffer that represents the prefix of
    the original buffer.

    @par Exception Safety

    No-throw guarantee.
*/
inline
net::const_buffer
buffers_prefix(std::size_t size,
    net::const_buffer buffer) noexcept
{
    return {buffer.data(), std::min<
        std::size_t>(size, buffer.size())};
}

/** Returns a prefix of a mutable buffer.

    The returned buffer points to the same memory as the passed
    buffer, but with a size that is equal to or smaller.

    @param size The maximum size of the returned buffer in bytes. If
    this is greater than or equal to the size of the passed buffer,
    the result will have the same size as the original buffer.

    @param buffer The buffer to return a prefix for. The
    underlying memory is not modified, and ownership of the
    memory is not transferred.

    @return A mutable buffer that represents the prefix of
    the original buffer.
*/
inline
net::mutable_buffer
buffers_prefix(std::size_t size,
    net::mutable_buffer buffer) noexcept
{
    return {buffer.data(), std::min<
        std::size_t>(size, buffer.size())};
}

/** Returns a prefix of a constant or mutable buffer sequence.

    The returned buffer sequence points to the same memory as the
    passed buffer sequence, but with a size that is equal to or
    smaller. No memory allocations are performed; the resulting
    sequence is calculated as a lazy range.

    @param size The maximum size of the returned buffer sequence
    in bytes. If this is greater than or equal to the size of
    the passed buffer sequence, the result will have the same
    size as the original buffer sequence.

    @param buffers An object whose type meets the requirements
    of <em>ConstBufferSequence</em>. The returned value will
    maintain a copy of the passed buffers for its lifetime;
    however, ownership of the underlying memory is not
    transferred.

    @return A constant buffer sequence that represents the prefix
    of the original buffer sequence. If the original buffer sequence
    also meets the requirements of <em>MutableBufferSequence</em>,
    then the returned value will also be a mutable buffer sequence.

    @note This function does not participate in overload resolution
    if `buffers` is convertible to either `net::const_buffer` or
    `net::mutable_buffer`.
*/
template<class ConstBufferSequence>
#if BOOST_BEAST_DOXYGEN
buffers_prefix_view<ConstBufferSequence>
#else
inline
typename std::enable_if<
    ! std::is_convertible<ConstBufferSequence,
        net::const_buffer>::value &&
    ! std::is_convertible<ConstBufferSequence,
        net::mutable_buffer>::value,
    buffers_prefix_view<ConstBufferSequence>>::type
#endif
buffers_prefix(std::size_t size, ConstBufferSequence const& buffers)
{
    static_assert(
        net::is_const_buffer_sequence<ConstBufferSequence>::value ||
        net::is_mutable_buffer_sequence<ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    return buffers_prefix_view<ConstBufferSequence>(size, buffers);
}

/** Returns the first buffer in a buffer sequence

    This returns the first buffer in the buffer sequence.
    If the buffer sequence is an empty range, the returned
    buffer will have a zero buffer size.

    @param buffers The buffer sequence. If the sequence is
    mutable, the returned buffer sequence will also be mutable.
    Otherwise, the returned buffer sequence will be constant.
*/
template<class BufferSequence>
typename std::conditional<
    net::is_mutable_buffer_sequence<BufferSequence>::value,
    net::mutable_buffer,
    net::const_buffer>::type
buffers_front(BufferSequence const& buffers)
{
    auto const first =
        net::buffer_sequence_begin(buffers);
    if(first == net::buffer_sequence_end(buffers))
        return {};
    return *first;
}

} // beast
} // boost

#include <boost/beast/core/impl/buffers_prefix.hpp>

#endif
