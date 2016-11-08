//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_PREPARE_BUFFERS_HPP
#define BEAST_PREPARE_BUFFERS_HPP

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

/** A buffer sequence adapter that shortens the sequence size.

    The class adapts a buffer sequence to efficiently represent
    a shorter subset of the original list of buffers starting
    with the first byte of the original sequence.

    @tparam BufferSequence The buffer sequence to adapt.
*/
template<class BufferSequence>
class prepared_buffers
{
    using iter_type =
        typename BufferSequence::const_iterator;

    BufferSequence bs_;
    iter_type back_;
    iter_type end_;
    std::size_t size_;

    template<class Deduced>
    prepared_buffers(Deduced&& other,
            std::size_t nback, std::size_t nend)
        : bs_(std::forward<Deduced>(other).bs_)
        , back_(std::next(bs_.begin(), nback))
        , end_(std::next(bs_.begin(), nend))
        , size_(other.size_)
    {
    }

    void
    setup(std::size_t n);

public:
    /// The type for each element in the list of buffers.
    using value_type = typename std::conditional<
        std::is_convertible<typename
            std::iterator_traits<iter_type>::value_type,
                boost::asio::mutable_buffer>::value,
                    boost::asio::mutable_buffer,
                        boost::asio::const_buffer>::type;

#if GENERATING_DOCS
    /// A bidirectional iterator type that may be used to read elements.
    using const_iterator = implementation_defined;

#else
    class const_iterator;

#endif

    /// Move constructor.
    prepared_buffers(prepared_buffers&&);

    /// Copy constructor.
    prepared_buffers(prepared_buffers const&);

    /// Move assignment.
    prepared_buffers& operator=(prepared_buffers&&);

    /// Copy assignment.
    prepared_buffers& operator=(prepared_buffers const&);

    /** Construct a shortened buffer sequence.

        @param n The maximum number of bytes in the wrapped
        sequence. If this is larger than the size of passed,
        buffers, the resulting sequence will represent the
        entire input sequence.

        @param buffers The buffer sequence to adapt. A copy of
        the sequence will be made, but ownership of the underlying
        memory is not transferred.
    */
    prepared_buffers(std::size_t n, BufferSequence const& buffers);

    /// Get a bidirectional iterator to the first element.
    const_iterator
    begin() const;

    /// Get a bidirectional iterator to one past the last element.
    const_iterator
    end() const;
};

//------------------------------------------------------------------------------

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
prepared_buffers<BufferSequence>
prepare_buffers(std::size_t n, BufferSequence const& buffers);

} // beast

#include <beast/core/impl/prepare_buffers.ipp>

#endif
