//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_STATIC_BUFFER_HPP
#define BEAST_STATIC_BUFFER_HPP

#include <beast/config.hpp>
#include <boost/utility/base_from_member.hpp>
#include <algorithm>
#include <array>
#include <cstring>

namespace beast {

/** A @b DynamicBuffer with a fixed size internal buffer.

    Ownership of the underlying storage belongs to the derived class.

    @note Variables are usually declared using the template class
    @ref static_buffer_n; however, to reduce the number of instantiations
    of template functions receiving static stream buffer arguments in a
    deduced context, the signature of the receiving function should use
    @ref static_buffer.

    When used with @ref static_buffer_n this implements a dynamic
    buffer using no memory allocations.

    @see @ref static_buffer_n
*/
class static_buffer
{
#if BEAST_DOXYGEN
private:
#else
protected:
#endif
    std::uint8_t* begin_;
    std::uint8_t* in_;
    std::uint8_t* out_;
    std::uint8_t* last_;
    std::uint8_t* end_;

public:
#if BEAST_DOXYGEN
    /// The type used to represent the input sequence as a list of buffers.
    using const_buffers_type = implementation_defined;

    /// The type used to represent the output sequence as a list of buffers.
    using mutable_buffers_type = implementation_defined;

#else
    class const_buffers_type;
    class mutable_buffers_type;

    static_buffer(
        static_buffer const& other) noexcept = delete;

    static_buffer& operator=(
        static_buffer const&) noexcept = delete;

#endif

    /// Return the size of the input sequence.
    std::size_t
    size() const
    {
        return out_ - in_;
    }

    /// Return the maximum sum of the input and output sequence sizes.
    std::size_t
    max_size() const
    {
        return end_ - begin_;
    }

    /// Return the maximum sum of input and output sizes that can be held without an allocation.
    std::size_t
    capacity() const
    {
        return end_ - in_;
    }

    /** Get a list of buffers that represent the input sequence.

        @note These buffers remain valid across subsequent calls to `prepare`.
    */
    const_buffers_type
    data() const;

    /** Get a list of buffers that represent the output sequence, with the given size.

        @throws std::length_error if the size would exceed the limit
        imposed by the underlying mutable buffer sequence.

        @note Buffers representing the input sequence acquired prior to
        this call remain valid.
    */
    mutable_buffers_type
    prepare(std::size_t n);

    /** Move bytes from the output sequence to the input sequence.

        @note Buffers representing the input sequence acquired prior to
        this call remain valid.
    */
    void
    commit(std::size_t n)
    {
        out_ += std::min<std::size_t>(n, last_ - out_);
    }

    /// Remove bytes from the input sequence.
    void
    consume(std::size_t n)
    {
        in_ += std::min<std::size_t>(n, out_ - in_);
    }

#if BEAST_DOXYGEN
private:
#else
protected:
#endif
    static_buffer(std::uint8_t* p, std::size_t n)
    {
        reset(p, n);
    }

    void
    reset(std::uint8_t* p, std::size_t n)
    {
        begin_ = p;
        in_ = p;
        out_ = p;
        last_ = p;
        end_ = p + n;
    }
};

//------------------------------------------------------------------------------

/** A @b DynamicBuffer with a fixed size internal buffer.

    This implements a dynamic buffer using no memory allocations.

    @tparam N The number of bytes in the internal buffer.

    @note To reduce the number of template instantiations when passing
    objects of this type in a deduced context, the signature of the
    receiving function should use @ref static_buffer instead.

    @see @ref static_buffer
*/
template<std::size_t N>
class static_buffer_n
    : public static_buffer
#if ! BEAST_DOXYGEN
    , private boost::base_from_member<
        std::array<std::uint8_t, N>>
#endif
{
    using member_type = boost::base_from_member<
        std::array<std::uint8_t, N>>;
public:
#if BEAST_DOXYGEN
private:
#endif
    static_buffer_n(
        static_buffer_n const&) = delete;
    static_buffer_n& operator=(
        static_buffer_n const&) = delete;
#if BEAST_DOXYGEN
public:
#endif

    /// Construct a static buffer.
    static_buffer_n()
        : static_buffer(
            member_type::member.data(),
                member_type::member.size())
    {
    }

    /** Reset the static buffer.

        @par Effects

        The input sequence and output sequence are empty,
        @ref max_size returns `N`.
    */
    void
    reset()
    {
        static_buffer::reset(
            member_type::member.data(),
                member_type::member.size());
    }
};

} // beast

#include <beast/core/impl/static_buffer.ipp>

#endif
