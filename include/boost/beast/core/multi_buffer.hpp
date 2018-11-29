//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_MULTI_BUFFER_HPP
#define BOOST_BEAST_MULTI_BUFFER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/allocator.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/core/empty_value.hpp>
#include <boost/intrusive/list.hpp>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>

namespace boost {
namespace beast {

/** A dynamic buffer providing sequences of variable length.

    A dynamic buffer encapsulates memory storage that may be
    automatically resized as required, where the memory is
    divided into two regions: readable bytes followed by
    writable bytes. These memory regions are internal to
    the dynamic buffer, but direct access to the elements
    is provided to permit them to be efficiently used with
    I/O operations.

    The implementation uses a sequence of one or more byte
    arrays of varying sizes to represent the readable and
    writable bytes. Additional byte array objects are
    appended to the sequence to accommodate changes in the
    desired size. The behavior and implementation of this
    container is most similar to `std::deque`.

    Objects of this type meet the requirements of @b DynamicBuffer
    and have the following additional properties:

    @li The buffer sequence representing the readable bytes
    returned by @ref data is mutable.

    @li Buffer sequences representing the readable and writable
    bytes, returned by @ref data and @ref prepare, may have
    length greater than one.

    @li A configurable maximum buffer size may be set upon
    construction. Attempts to exceed the buffer size will throw
    `std::length_error`.

    @li Sequences previously obtained using @ref data remain
    valid after calls to @ref prepare or @ref commit.

    @tparam Allocator The allocator to use for managing memory.
*/
template<class Allocator>
class basic_multi_buffer
#if ! BOOST_BEAST_DOXYGEN
    : private boost::empty_value<
        typename detail::allocator_traits<Allocator>::
            template rebind_alloc<char>>
#endif
{
    using base_alloc_type = typename
        detail::allocator_traits<Allocator>::
            template rebind_alloc<char>;

    // Storage for the list of buffers representing the input
    // and output sequences. The allocation for each element
    // contains `element` followed by raw storage bytes.
    class element;

    template<bool IsMutable>
    class readable_bytes;

    using alloc_traits = detail::allocator_traits<base_alloc_type>;
    using list_type = typename boost::intrusive::make_list<element,
        boost::intrusive::constant_time_size<true>>::type;
    using iter = typename list_type::iterator;
    using const_iter = typename list_type::const_iterator;

    using size_type = typename alloc_traits::size_type;
    using const_buffer = boost::asio::const_buffer;
    using mutable_buffer = boost::asio::mutable_buffer;

    static_assert(std::is_base_of<std::bidirectional_iterator_tag,
        typename std::iterator_traits<iter>::iterator_category>::value,
            "BidirectionalIterator requirements not met");

    static_assert(std::is_base_of<std::bidirectional_iterator_tag,
        typename std::iterator_traits<const_iter>::iterator_category>::value,
            "BidirectionalIterator requirements not met");

    std::size_t max_ =
        (std::numeric_limits<std::size_t>::max)();
    list_type list_;        // list of allocated buffers
    iter out_;              // element that contains out_pos_
    size_type in_size_ = 0; // size of the input sequence
    size_type in_pos_ = 0;  // input offset in list_.front()
    size_type out_pos_ = 0; // output offset in *out_
    size_type out_end_ = 0; // output end offset in list_.back()

public:
    /// The type of allocator used.
    using allocator_type = Allocator;

    /// Destructor
    ~basic_multi_buffer();

    /** Constructor

        Upon construction, @ref capacity will return zero.
    */
    basic_multi_buffer() noexcept(
        std::is_nothrow_default_constructible<Allocator>::value)
        :
        out_(list_.end())
    {
    }

    /** Constructor

        Upon construction, @ref capacity will return zero.

        @param limit The setting for @ref max_size.
    */
    explicit
    basic_multi_buffer(std::size_t limit) noexcept(
        std::is_nothrow_default_constructible<Allocator>::value)
        : max_(limit)
        , out_(list_.end())
    {
    }

    /** Constructor

        Upon construction, @ref capacity will return zero.

        @param alloc The allocator to use.
    */
    explicit
    basic_multi_buffer(Allocator const& alloc) noexcept;

    /** Constructor

        Upon construction, @ref capacity will return zero.

        @param limit The setting for @ref max_size.

        @param alloc The allocator to use.
    */
    basic_multi_buffer(
        std::size_t limit, Allocator const& alloc) noexcept;

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
    basic_multi_buffer(basic_multi_buffer&& other) noexcept;

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
    basic_multi_buffer(basic_multi_buffer&& other,
        Allocator const& alloc);

    /** Copy Constructor

        The newly constructed object will have a copy of the
        allocator and contents of other, and zero writable bytes.

        @param other The object to copy from.
    */
    basic_multi_buffer(basic_multi_buffer const& other);

    /** Copy Constructor

        The newly constructed object will have a copy of the
        specified allocator, a copy of the contents of other,
        and zero writable bytes.

        @param other The object to copy from.

        @param alloc The allocator to use.
    */
    basic_multi_buffer(basic_multi_buffer const& other,
        Allocator const& alloc);

    /** Copy Constructor

        The newly constructed object will have a copy of the
        contents of other, and zero writable bytes.

        @param other The object to copy from.
    */
    template<class OtherAlloc>
    basic_multi_buffer(basic_multi_buffer<
        OtherAlloc> const& other);

    /** Copy Constructor

        The newly constructed object will have a copy of the
        specified allocator, a copy of the contents of other,
        and zero writable bytes.

        @param other The object to copy from.

        @param alloc The allocator to use.
    */
    template<class OtherAlloc>
    basic_multi_buffer(basic_multi_buffer<
        OtherAlloc> const& other, allocator_type const& alloc);

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
    basic_multi_buffer&
    operator=(basic_multi_buffer&& other);

    /** Copy Assignment

        The assigned object will have a copy of the allocator
        and contents of other, and zero writable bytes. The
        previous contents of this container are deleted.

        @param other The object to copy from.
    */
    basic_multi_buffer& operator=(basic_multi_buffer const& other);

    /** Copy assignment

        The assigned object will have a copy of the contents
        of other, and zero writable bytes. The previous contents
        of this container are deleted.

        @param other The object to copy from.
    */
    template<class OtherAlloc>
    basic_multi_buffer& operator=(
        basic_multi_buffer<OtherAlloc> const& other);

    /// Returns a copy of the allocator used.
    allocator_type
    get_allocator() const
    {
        return this->get();
    }

    //--------------------------------------------------------------------------

#if BOOST_BEAST_DOXYGEN
    /// The ConstBufferSequence used to represent the readable bytes.
    using const_buffers_type = __implementation_defined__;

    /// The MutableBufferSequence used to represent the readable bytes.
    using mutable_data_type = __implementation_defined__;

    /// The MutableBufferSequence used to represent the writable bytes.
    using mutable_buffers_type = __implementation_defined__;
#else
    using const_buffers_type = readable_bytes<false>;
    using mutable_data_type = readable_bytes<true>;
    class mutable_buffers_type;
#endif

    /// Returns the number of readable bytes.
    size_type
    size() const noexcept
    {
        return in_size_;
    }

    /// Return the maximum number of bytes, both readable and writable, that can ever be held.
    size_type
    max_size() const noexcept
    {
        return max_;
    }

    /// Return the maximum number of bytes, both readable and writable, that can be held without requiring an allocation.
    std::size_t
    capacity() const noexcept;

    /// Returns a constant buffer sequence representing the readable bytes
    const_buffers_type
    data() const noexcept;

    /// Returns a constant buffer sequence representing the readable bytes
    const_buffers_type
    cdata() const noexcept
    {
        return data();
    }

    /** Returns a mutable buffer sequence representing the readable bytes.

        @note The sequence may contain zero or more contiguous memory
        regions.
    */
    mutable_data_type
    data() noexcept;

    /** Returns a mutable buffer sequence representing writable bytes.
    
        Returns a mutable buffer sequence representing the writable
        bytes containing exactly `n` bytes of storage. Memory may be
        reallocated as needed.

        All buffer sequences previously obtained using @ref prepare are
        invalidated. Buffer sequences previously obtained using @ref data
        remain valid.

        @param n The desired number of bytes in the returned buffer
        sequence.

        @throws std::length_error if `size() + n` exceeds `max_size()`.
    */
    mutable_buffers_type
    prepare(size_type n);

    /** Append writable bytes to the readable bytes.

        Appends n bytes from the start of the writable bytes to the
        end of the readable bytes. The remainder of the writable bytes
        are discarded. If n is greater than the number of writable
        bytes, all writable bytes are appended to the readable bytes.

        All buffer sequences previously obtained using @ref prepare are
        invalidated. Buffer sequences previously obtained using @ref data
        remain valid.

        @param n The number of bytes to append. If this number
        is greater than the number of writable bytes, all
        writable bytes are appended.
    */
    void
    commit(size_type n) noexcept;

    /** Remove bytes from beginning of the readable bytes.

        Removes n bytes from the beginning of the readable bytes.

        All buffers sequences previously obtained using
        @ref data or @ref prepare are invalidated.

        @param n The number of bytes to remove. If this number
        is greater than the number of readable bytes, all
        readable bytes are removed.
    */
    void
    consume(size_type n) noexcept;

    /// Exchange two dynamic buffers
    template<class Alloc>
    friend
    void
    swap(
        basic_multi_buffer<Alloc>& lhs,
        basic_multi_buffer<Alloc>& rhs) noexcept;

private:
    template<class OtherAlloc>
    friend class basic_multi_buffer;

    void reset() noexcept;
    void delete_list() noexcept;

    template<class DynamicBuffer>
    void copy_from(DynamicBuffer const& other);
    void move_assign(basic_multi_buffer& other, std::false_type);
    void move_assign(basic_multi_buffer& other, std::true_type) noexcept;
    void copy_assign(basic_multi_buffer const& other, std::false_type);
    void copy_assign(basic_multi_buffer const& other, std::true_type);
    void swap(basic_multi_buffer&) noexcept;
    void swap(basic_multi_buffer&, std::true_type) noexcept;
    void swap(basic_multi_buffer&, std::false_type) noexcept;
    void debug_check() const;
};

/// A typical multi buffer
using multi_buffer = basic_multi_buffer<std::allocator<char>>;

} // beast
} // boost

#include <boost/beast/core/impl/multi_buffer.ipp>

#endif