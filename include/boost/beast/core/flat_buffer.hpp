//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_FLAT_BUFFER_HPP
#define BOOST_BEAST_FLAT_BUFFER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/allocator.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/core/empty_value.hpp>
#include <limits>
#include <memory>

namespace boost {
namespace beast {

/** A dynamic buffer providing buffer sequences of length one.

    A dynamic buffer encapsulates memory storage that may be
    automatically resized as required, where the memory is
    divided into two regions: readable bytes followed by
    writable bytes. These memory regions are internal to
    the dynamic buffer, but direct access to the elements
    is provided to permit them to be efficiently used with
    I/O operations.

    Objects of this type meet the requirements of @b DynamicBuffer
    and have the following additional properties:

    @li A mutable buffer sequence representing the readable
    bytes is returned by @ref data when `this` is non-const.

    @li A configurable maximum buffer size may be set upon
    construction. Attempts to exceed the buffer size will throw
    `std::length_error`.

    @li Buffer sequences representing the readable and writable
    bytes, returned by @ref data and @ref prepare, will have
    length one.

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
#if ! BOOST_BEAST_DOXYGEN
    : private boost::empty_value<
        typename detail::allocator_traits<Allocator>::
            template rebind_alloc<char>>
#endif
{
    enum
    {
        min_size = 512
    };

    template<class OtherAlloc>
    friend class basic_flat_buffer;

    using base_alloc_type = typename
        detail::allocator_traits<Allocator>::
            template rebind_alloc<char>;

    using alloc_traits =
        detail::allocator_traits<base_alloc_type>;

    static
    std::size_t
    dist(char const* first, char const* last)
    {
        return static_cast<std::size_t>(last - first);
    }

    char* begin_;
    char* in_;
    char* out_;
    char* last_;
    char* end_;
    std::size_t max_;

public:
    /// The type of allocator used.
    using allocator_type = Allocator;

    /// Destructor
    ~basic_flat_buffer();

    /** Constructor

        Upon construction, @ref capacity will return zero.
    */
    basic_flat_buffer();

    /** Constructor

        Upon construction, @ref capacity will return zero.

        @param limit The setting for @ref max_size.
    */
    explicit
    basic_flat_buffer(std::size_t limit);

    /** Constructor

        Upon construction, @ref capacity will return zero.

        @param alloc The allocator to construct with.
    */
    explicit
    basic_flat_buffer(Allocator const& alloc);

    /** Constructor

        Upon construction, @ref capacity will return zero.

        @param limit The setting for @ref max_size.

        @param alloc The allocator to use.
    */
    basic_flat_buffer(
        std::size_t limit, Allocator const& alloc);

    /** Move Constructor

        Constructs the container with the contents of other
        using move semantics. After the move, other is
        guaranteed to be empty.

        Buffer sequences previously obtained using @ref data
        or @ref prepare are not invalidated after the move.

        @param other The object to move from. After the move,
        the moved-from object's state will be as if default
        constructed using its current allocator and limit.
    */
    basic_flat_buffer(basic_flat_buffer&& other);

    /** Move Constructor

        Using alloc as the allocator for the new container, the
        contents of other are moved. If `alloc != other.get_allocator()`,
        this results in a copy. After the move, other is
        guaranteed to be empty.

        All buffers sequences previously obtained using
        @ref data or @ref prepare are invalidated.

        @param other The object to move from. After the move,
        the moved-from object's state will be as if default
        constructed using its current allocator and limit.

        @param alloc The allocator to use for the newly
        constructed object.
    */
    basic_flat_buffer(
        basic_flat_buffer&& other, Allocator const& alloc);

    /** Copy Constructor

        The newly constructed object will have a copy of the
        allocator and contents of other, and zero writable bytes.

        @param other The object to copy from.
    */
    basic_flat_buffer(basic_flat_buffer const& other);

    /** Copy Constructor

        The newly constructed object will have a copy of the
        specified allocator, a copy of the contents of other,
        and zero writable bytes.

        @param other The object to copy from.

        @param alloc The allocator to use.
    */
    basic_flat_buffer(basic_flat_buffer const& other,
        Allocator const& alloc);

    /** Copy Constructor

        The newly constructed object will have a copy of the
        contents of other, and zero writable bytes.

        @param other The object to copy from.
    */
    template<class OtherAlloc>
    basic_flat_buffer(
        basic_flat_buffer<OtherAlloc> const& other);

    /** Copy Constructor

        The newly constructed object will have a copy of the
        specified allocator, a copy of the contents of other,
        and zero writable bytes.

        @param other The object to copy from.

        @param alloc The allocator to use.
    */
    template<class OtherAlloc>
    basic_flat_buffer(
        basic_flat_buffer<OtherAlloc> const& other,
            Allocator const& alloc);

    /** Move Assignment

        Assigns the container with the contents of other
        using move semantics. After the move, other is
        guaranteed to be empty. The previous contents of
        this container are deleted.

        Buffer sequences previously obtained using @ref data
        or @ref prepare are not invalidated after the move.

        @param other The object to move from. After the move,
        the moved-from object's state will be as if default
        constructed using its current allocator and limit.
    */
    basic_flat_buffer&
    operator=(basic_flat_buffer&& other);

    /** Copy Assignment

        The assigned object will have a copy of the allocator
        and contents of other, and zero writable bytes. The
        previous contents of this container are deleted.

        @param other The object to copy from.
    */
    basic_flat_buffer&
    operator=(basic_flat_buffer const& other);

    /** Copy assignment

        The assigned object will have a copy of the contents
        of other, and zero writable bytes. The previous contents
        of this container are deleted.

        @param other The object to copy from.
    */
    template<class OtherAlloc>
    basic_flat_buffer&
    operator=(basic_flat_buffer<OtherAlloc> const& other);

    /// Returns a copy of the allocator used.
    allocator_type
    get_allocator() const
    {
        return this->get();
    }

    /** Reallocate the buffer to fit the readable bytes exactly.

        All buffers sequences previously obtained using
        @ref data or @ref prepare are invalidated.
    */
    void
    shrink_to_fit();

    /// Exchange two dynamic buffers
    template<class Alloc>
    friend
    void
    swap(
        basic_flat_buffer<Alloc>&,
        basic_flat_buffer<Alloc>&);

    //--------------------------------------------------------------------------

    /// The ConstBufferSequence used to represent the readable bytes.
    using const_buffers_type = net::const_buffer;

    /// The MutableBufferSequence used to represent the readable bytes.
    using mutable_data_type = net::mutable_buffer;

    /// The MutableBufferSequence used to represent the writable bytes.
    using mutable_buffers_type = net::mutable_buffer;

    /// Returns the number of readable bytes.
    std::size_t
    size() const noexcept
    {
        return dist(in_, out_);
    }

    /// Return the maximum number of bytes, both readable and writable, that can ever be held.
    std::size_t
    max_size() const noexcept
    {
        return max_;
    }

    /// Return the maximum number of bytes, both readable and writable, that can be held without requiring an allocation.
    std::size_t
    capacity() const noexcept
    {
        return dist(begin_, end_);
    }

    /// Returns a constant buffer sequence representing the readable bytes
    const_buffers_type
    data() const noexcept
    {
        return {in_, dist(in_, out_)};
    }

    /// Returns a constant buffer sequence representing the readable bytes
    const_buffers_type
    cdata() const noexcept
    {
        return data();
    }

    /// Returns a mutable buffer sequence representing the readable bytes
    mutable_data_type
    data() noexcept
    {
        return {in_, dist(in_, out_)};
    }

    /** Returns a mutable buffer sequence representing writable bytes.
    
        Returns a mutable buffer sequence representing the writable
        bytes containing exactly `n` bytes of storage. Memory may be
        reallocated as needed.

        All buffers sequences previously obtained using
        @ref data or @ref prepare are invalidated.

        @param n The desired number of bytes in the returned buffer
        sequence.

        @throws std::length_error if `size() + n` exceeds `max_size()`.
    */
    mutable_buffers_type
    prepare(std::size_t n);

    /** Append writable bytes to the readable bytes.

        Appends n bytes from the start of the writable bytes to the
        end of the readable bytes. The remainder of the writable bytes
        are discarded. If n is greater than the number of writable
        bytes, all writable bytes are appended to the readable bytes.

        All buffers sequences previously obtained using
        @ref data or @ref prepare are invalidated.

        @param n The number of bytes to append. If this number
        is greater than the number of writable bytes, all
        writable bytes are appended.
    */
    void
    commit(std::size_t n) noexcept
    {
        out_ += (std::min)(n, dist(out_, last_));
    }

    /** Remove bytes from beginning of the readable bytes.

        Removes n bytes from the beginning of the readable bytes.

        All buffers sequences previously obtained using
        @ref data or @ref prepare are invalidated.

        @param n The number of bytes to remove. If this number
        is greater than the number of readable bytes, all
        readable bytes are removed.
    */
    void
    consume(std::size_t n) noexcept;

private:
    void
    reset();

    template<class DynamicBuffer>
    void copy_from(DynamicBuffer const& other);
    void move_assign(basic_flat_buffer&, std::true_type);
    void move_assign(basic_flat_buffer&, std::false_type);
    void copy_assign(basic_flat_buffer const&, std::true_type);
    void copy_assign(basic_flat_buffer const&, std::false_type);
    void swap(basic_flat_buffer&);
    void swap(basic_flat_buffer&, std::true_type);
    void swap(basic_flat_buffer&, std::false_type);
};

/// A flat buffer which uses the default allocator.
using flat_buffer =
    basic_flat_buffer<std::allocator<char>>;

} // beast
} // boost

#include <boost/beast/core/impl/flat_buffer.ipp>

#endif
