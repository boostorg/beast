//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_STATIC_BUFFER_HPP
#define BOOST_BEAST_STATIC_BUFFER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/assert.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>

namespace boost {
namespace beast {

namespace detail {

template<bool IsMutable>
class buffers_pair
{
    template<bool IsMutable_>
    friend class buffers_pair;

public:
    // VFALCO: This type is public otherwise
    //         asio::buffers_iterator won't compile.
    using value_type = typename
        std::conditional<IsMutable,
            net::mutable_buffer,
            net::const_buffer>::type;

    using const_iterator =
        value_type const*;

    buffers_pair() = default;
    buffers_pair(buffers_pair const&) = default;
    buffers_pair& operator=(buffers_pair const&) = default;

    template<
        bool IsMutable_ = IsMutable,
        class = typename std::enable_if<! IsMutable_>::type>
    buffers_pair(buffers_pair<true> const& other) noexcept
        : b_{other.b_[0], other.b_[1]}
    {
    }

    template<
        bool IsMutable_ = IsMutable,
        class = typename std::enable_if<! IsMutable_>::type>
    buffers_pair& operator=(
        buffers_pair<true> const& other) noexcept
    {
        b_ = {other.b_[0], other.b_[1]};
        return *this;
    }

    value_type&
    operator[](int i) noexcept
    {
        BOOST_ASSERT(i >= 0 && i < 2);
        return b_[i];
    }

    const_iterator
    begin() const noexcept
    {
        return &b_[0];
    }

    const_iterator
    end() const noexcept
    {
        if(b_[1].size() > 0)
            return &b_[2];
        else
            return &b_[1];
    }
private:
    value_type b_[2];
};

} // detail

/** A dynamic buffer providing a fixed, circular buffer.

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

    @li Buffer sequences representing the readable and writable
    bytes, returned by @ref data and @ref prepare, will have
    length at most one.

    @li All operations execute in constant time.

    @li Ownership of the underlying storage belongs to the
    derived class.

    @note Variables are usually declared using the template class
    @ref static_buffer; however, to reduce the number of template
    instantiations, objects should be passed `static_buffer_base&`.

    @see @ref static_buffer
*/
class static_buffer_base
{
    char* begin_;
    std::size_t in_off_ = 0;
    std::size_t in_size_ = 0;
    std::size_t out_size_ = 0;
    std::size_t capacity_;

    static_buffer_base(static_buffer_base const& other) = delete;
    static_buffer_base& operator=(static_buffer_base const&) = delete;

public:
    /** Constructor

        This creates a dynamic buffer using the provided storage area.

        @param p A pointer to valid storage of at least `n` bytes.

        @param size The number of valid bytes pointed to by `p`.
    */
    static_buffer_base(void* p, std::size_t size) noexcept;

    //--------------------------------------------------------------------------

#if BOOST_BEAST_DOXYGEN
    /// The ConstBufferSequence used to represent the readable bytes.
    using const_buffers_type = __implementation_defined__;

    /// The MutableBufferSequence used to represent the readable bytes.
    using mutable_data_type = __implementation_defined__;

    /// The MutableBufferSequence used to represent the writable bytes.
    using mutable_buffers_type = __implementation_defined__;
#else
    using const_buffers_type   = detail::buffers_pair<false>;
    using mutable_data_type    = detail::buffers_pair<true>;
    using mutable_buffers_type = detail::buffers_pair<true>;
#endif

    /// Returns the number of readable bytes.
    std::size_t
    size() const noexcept
    {
        return in_size_;
    }

    /// Return the maximum number of bytes, both readable and writable, that can ever be held.
    std::size_t
    max_size() const noexcept
    {
        return capacity_;
    }

    /// Return the maximum number of bytes, both readable and writable, that can be held without requiring an allocation.
    std::size_t
    capacity() const noexcept
    {
        return capacity_;
    }

    /// Returns a constant buffer sequence representing the readable bytes
    const_buffers_type
    data() const noexcept;
    
    /// Returns a constant buffer sequence representing the readable bytes
    const_buffers_type
    cdata() const noexcept
    {
        return data();
    }

    /// Returns a mutable buffer sequence representing the readable bytes
    mutable_data_type
    data() noexcept;

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
    commit(std::size_t n) noexcept;

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

protected:
    /** Constructor

        The buffer will be in an undefined state. It is necessary
        for the derived class to call @ref reset in order to
        initialize the object.
    */
    static_buffer_base() = default;

    /** Reset the pointed-to buffer.

        This function resets the internal state to the buffer provided.
        All input and output sequences are invalidated. This function
        allows the derived class to construct its members before
        initializing the static buffer.

        @param p A pointer to valid storage of at least `n` bytes.

        @param size The number of valid bytes pointed to by `p`.
    */
    void
    reset(void* p, std::size_t size) noexcept;
};

//------------------------------------------------------------------------------

/** A circular @b DynamicBuffer with a fixed size internal buffer.

    This implements a circular dynamic buffer. Calls to @ref prepare
    never require moving memory. The buffer sequences returned may
    be up to length two.
    Ownership of the underlying storage belongs to the derived class.

    @tparam N The number of bytes in the internal buffer.

    @note To reduce the number of template instantiations when passing
    objects of this type in a deduced context, the signature of the
    receiving function should use @ref static_buffer_base instead.

    @see @ref static_buffer_base
*/
template<std::size_t N>
class static_buffer : public static_buffer_base
{
    char buf_[N];

public:
    /// Constructor
    static_buffer(static_buffer const&);

    /// Constructor
    static_buffer()
        : static_buffer_base(buf_, N)
    {
    }

    /// Assignment
    static_buffer& operator=(static_buffer const&);

    /// Returns the @ref static_buffer_base portion of this object
    static_buffer_base&
    base()
    {
        return *this;
    }

    /// Returns the @ref static_buffer_base portion of this object
    static_buffer_base const&
    base() const
    {
        return *this;
    }

    /// Return the maximum sum of the input and output sequence sizes.
    std::size_t constexpr
    max_size() const
    {
        return N;
    }

    /// Return the maximum sum of input and output sizes that can be held without an allocation.
    std::size_t constexpr
    capacity() const
    {
        return N;
    }
};

} // beast
} // boost

#include <boost/beast/core/impl/static_buffer.ipp>

#endif
