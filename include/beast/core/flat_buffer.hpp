//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_FLAT_BUFFER_HPP
#define BEAST_FLAT_BUFFER_HPP

#include <beast/config.hpp>
#include <beast/core/detail/empty_base_optimization.hpp>
#include <boost/asio/buffer.hpp>
#include <limits>
#include <memory>

namespace beast {

/** A linear dynamic buffer.

    Objects of this type meet the requirements of @b DynamicBuffer
    and offer additional invariants:
    
    @li Buffer sequences returned by @ref data and @ref prepare
    will always be of length one.

    @li A configurable maximum buffer size may be set upon
    construction. Attempts to exceed the buffer size will throw
    `std::length_error`.

    Upon construction, a maximum size for the buffer may be
    specified. If this limit is exceeded, the `std::length_error`
    exception will be thrown.

    @note This class is designed for use with algorithms that
    take dynamic buffers as parameters, and are optimized
    for the case where the input sequence or output sequence
    is stored in a single contiguous buffer.
*/
template<class Allocator>
class basic_flat_buffer
#if ! BEAST_DOXYGEN
    : private detail::empty_base_optimization<
        typename std::allocator_traits<Allocator>::
            template rebind_alloc<char>>
#endif
{
public:
#if BEAST_DOXYGEN
    /// The type of allocator used.
    using allocator_type = Allocator;
#else
    using allocator_type = typename
        std::allocator_traits<Allocator>::
            template rebind_alloc<char>;
#endif

private:
    enum
    {
        min_size = 512
    };

    template<class OtherAlloc>
    friend class basic_flat_buffer;

    using alloc_traits =
        std::allocator_traits<allocator_type>;

    static
    inline
    std::size_t
    dist(char const* first, char const* last)
    {
        return static_cast<std::size_t>(last - first);
    }

    char* p_;
    char* in_;
    char* out_;
    char* last_;
    char* end_;
    std::size_t max_;

public:
    /// The type used to represent the input sequence as a list of buffers.
    using const_buffers_type = boost::asio::const_buffers_1;

    /// The type used to represent the output sequence as a list of buffers.
    using mutable_buffers_type = boost::asio::mutable_buffers_1;

    /// Copy assignment (disallowed).
    basic_flat_buffer&
    operator=(basic_flat_buffer const&) = delete;

    /// Destructor.
    ~basic_flat_buffer();

    /** Move constructor.

        The new object will have the same input sequence
        and an empty output sequence.

        @note After the move, the moved-from object will
        have a capacity of zero, an empty input sequence,
        and an empty output sequence.
    */
    basic_flat_buffer(basic_flat_buffer&&);

    /** Move constructor.

        The new object will have the same input sequence
        and an empty output sequence.

        @note After the move, the moved-from object will
        have a capacity of zero, an empty input sequence,
        and an empty output sequence.

        @param alloc The allocator to associate with the
        stream buffer.
    */
    basic_flat_buffer(basic_flat_buffer&&,
        Allocator const& alloc);
    
    /** Copy constructor.

        The new object will have a copy of the input sequence
        and an empty output sequence.
    */
    basic_flat_buffer(basic_flat_buffer const&);

    /** Copy constructor.

        The new object will have a copy of the input sequence
        and an empty output sequence.

        @param alloc The allocator to associate with the
        stream buffer.
    */
    basic_flat_buffer(basic_flat_buffer const&,
        Allocator const& alloc);
    
    /** Copy constructor.

        The new object will have a copy of the input sequence
        and an empty output sequence.
    */
    template<class OtherAlloc>
    basic_flat_buffer(
        basic_flat_buffer<OtherAlloc> const&);

    /** Copy constructor.

        The new object will have a copy of the input sequence
        and an empty output sequence.

        @param alloc The allocator to associate with the
        stream buffer.
    */
    template<class OtherAlloc>
    basic_flat_buffer(
        basic_flat_buffer<OtherAlloc> const&,
            Allocator const& alloc);

    /** Construct a flat stream buffer.

        No allocation is performed; the buffer will have
        empty input and output sequences.

        @param limit An optional non-zero value specifying the
        maximum of the sum of the input and output sequence sizes
        that can be allocated. If unspecified, the largest
        possible value of `std::size_t` is used.
    */
    explicit
    basic_flat_buffer(std::size_t limit = (
        std::numeric_limits<std::size_t>::max)());

    /** Construct a flat stream buffer.

        No allocation is performed; the buffer will have
        empty input and output sequences.

        @param alloc The allocator to associate with the
        stream buffer.

        @param limit An optional non-zero value specifying the
        maximum of the sum of the input and output sequence sizes
        that can be allocated. If unspecified, the largest
        possible value of `std::size_t` is used.
    */
    basic_flat_buffer(Allocator const& alloc,
        std::size_t limit = (
            std::numeric_limits<std::size_t>::max)());

    /// Returns a copy of the associated allocator.
    allocator_type
    get_allocator() const
    {
        return this->member();
    }

    /// Returns the size of the input sequence.
    std::size_t
    size() const
    {
        return dist(in_, out_);
    }

    /// Return the maximum sum of the input and output sequence sizes.
    std::size_t
    max_size() const
    {
        return max_;
    }

    /// Return the maximum sum of input and output sizes that can be held without an allocation.
    std::size_t
    capacity() const
    {
        return dist(p_, end_);
    }

    /// Get a list of buffers that represent the input sequence.
    const_buffers_type
    data() const
    {
        return {in_, dist(in_, out_)};
    }

    /** Get a list of buffers that represent the output sequence, with the given size.

        @throws std::length_error if `size() + n` exceeds `max_size()`.

        @note All previous buffers sequences obtained from
        calls to @ref data or @ref prepare are invalidated.
    */
    mutable_buffers_type
    prepare(std::size_t n);

    /** Move bytes from the output sequence to the input sequence.

        @param n The number of bytes to move. If this is larger than
        the number of bytes in the output sequences, then the entire
        output sequences is moved.

        @note All previous buffers sequences obtained from
        calls to @ref data or @ref prepare are invalidated.
    */
    void
    commit(std::size_t n)
    {
        out_ += (std::min)(n, dist(out_, last_));
    }

    /** Remove bytes from the input sequence.

        If `n` is greater than the number of bytes in the input
        sequence, all bytes in the input sequence are removed.

        @note All previous buffers sequences obtained from
        calls to @ref data or @ref prepare are invalidated.
    */
    void
    consume(std::size_t n);

    /** Reserve space in the stream.

        This reallocates the buffer if necessary.

        @note All previous buffers sequences obtained from
        calls to @ref data or @ref prepare are invalidated.

        @param n The number of bytes to reserve. Upon success,
        the capacity will be at least `n`. 

        @throws std::length_error if `n` exceeds `max_size()`.
    */
    void
    reserve(std::size_t n);

    /** Reallocate the buffer to fit the input sequence.

        @note All previous buffers sequences obtained from
        calls to @ref data or @ref prepare are invalidated.
    */
    void
    shrink_to_fit();

private:
    void
    move_from(basic_flat_buffer& other);

    template<class OtherAlloc>
    void
    copy_from(basic_flat_buffer<
        OtherAlloc> const& other);
};

using flat_buffer =
    basic_flat_buffer<std::allocator<char>>;

} // beast

#include <beast/core/impl/flat_buffer.ipp>

#endif
