//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_IMPL_FLAT_BUFFER_HPP
#define BEAST_IMPL_FLAT_BUFFER_HPP

#include <boost/assert.hpp>
#include <boost/throw_exception.hpp>
#include <stdexcept>

namespace beast {

/*  Memory is laid out thusly:

    p_ ..|.. in_ ..|.. out_ ..|.. last_ ..|.. end_
*/

namespace detail {

inline
std::size_t
next_pow2(std::size_t x)
{
    std::size_t n = 0;
    while(x > 0)
    {
        ++n;
        x >>= 1;
    }
    return std::size_t{1} << n;
}

} // detail

template<class Allocator>
void
basic_flat_buffer<Allocator>::
move_from(basic_flat_buffer& other)
{
    p_ = other.p_;
    in_ = other.in_;
    out_ = other.out_;
    last_ = out_;
    end_ = other.end_;
    max_ = other.max_;
    other.p_ = nullptr;
    other.in_ = nullptr;
    other.out_ = nullptr;
    other.last_ = nullptr;
    other.end_ = nullptr;
}

template<class Allocator>
template<class OtherAlloc>
void
basic_flat_buffer<Allocator>::
copy_from(basic_flat_buffer<
    OtherAlloc> const& other)
{
    max_ = other.max_;
    auto const n = other.size();
    if(n > 0)
    {
        p_ = alloc_traits::allocate(
            this->member(), n);
        in_ = p_;
        out_ = p_ + n;
        last_ = out_;
        end_ = out_;
        std::memcpy(in_, other.in_, n);
        return;
    }
    p_ = nullptr;
    in_ = nullptr;
    out_ = nullptr;
    last_ = nullptr;
    end_ = nullptr;
}

template<class Allocator>
basic_flat_buffer<Allocator>::
~basic_flat_buffer()
{
    if(p_)
        alloc_traits::deallocate(
            this->member(), p_, dist(p_, end_));
}

template<class Allocator>
basic_flat_buffer<Allocator>::
basic_flat_buffer(basic_flat_buffer&& other)
    : detail::empty_base_optimization<
        allocator_type>(std::move(other.member()))
{
    move_from(other);
}

template<class Allocator>
basic_flat_buffer<Allocator>::
basic_flat_buffer(basic_flat_buffer&& other,
        Allocator const& alloc)
    : detail::empty_base_optimization<
        allocator_type>(alloc)
{
    if(this->member() != other.member())
    {
        copy_from(other);
        return;
    }
    move_from(other);
}

template<class Allocator>
basic_flat_buffer<Allocator>::
basic_flat_buffer(
        basic_flat_buffer const& other)
    : detail::empty_base_optimization<allocator_type>(
        alloc_traits::select_on_container_copy_construction(
            other.member()))
{
    copy_from(other);
}

template<class Allocator>
basic_flat_buffer<Allocator>::
basic_flat_buffer(
    basic_flat_buffer const& other,
        Allocator const& alloc)
    : detail::empty_base_optimization<
        allocator_type>(alloc)
{
    copy_from(other);
}
    
template<class Allocator>
template<class OtherAlloc>
basic_flat_buffer<Allocator>::
basic_flat_buffer(
    basic_flat_buffer<OtherAlloc> const& other)
{
    copy_from(other);
}

template<class Allocator>
template<class OtherAlloc>
basic_flat_buffer<Allocator>::
basic_flat_buffer(
    basic_flat_buffer<OtherAlloc> const& other,
        Allocator const& alloc)
    : detail::empty_base_optimization<
        allocator_type>(alloc)
{
    copy_from(other);
}

template<class Allocator>
basic_flat_buffer<Allocator>::
basic_flat_buffer(std::size_t limit)
    : p_(nullptr)
    , in_(nullptr)
    , out_(nullptr)
    , last_(nullptr)
    , end_(nullptr)
    , max_(limit)
{
    BOOST_ASSERT(limit >= 1);
}

template<class Allocator>
basic_flat_buffer<Allocator>::
basic_flat_buffer(Allocator const& alloc,
        std::size_t limit)
    : detail::empty_base_optimization<
        allocator_type>(alloc)
    , p_(nullptr)
    , in_(nullptr)
    , out_(nullptr)
    , last_(nullptr)
    , end_(nullptr)
    , max_(limit)
{
    BOOST_ASSERT(limit >= 1);
}

template<class Allocator>
auto
basic_flat_buffer<Allocator>::
prepare(std::size_t n) ->
    mutable_buffers_type
{
    if(n <= dist(out_, end_))
    {
        // existing capacity is sufficient
        last_ = out_ + n;
        return{out_, n};
    }
    auto const len = size();
    if(n <= capacity() - len)
    {
        // after a memmove,
        // existing capacity is sufficient
        if(len > 0)
            std::memmove(p_, in_, len);
        in_ = p_;
        out_ = in_ + len;
        last_ = out_ + n;
        return {out_, n};
    }
    // enforce maximum capacity
    if(n > max_ - len)
        BOOST_THROW_EXCEPTION(std::length_error{
            "basic_flat_buffer overflow"});
    // allocate a new buffer
    auto const new_size = (std::min)(max_,
        std::max<std::size_t>(
            detail::next_pow2(len + n), min_size));
    auto const p = alloc_traits::allocate(
        this->member(), new_size);
    if(len > 0)
    {
        BOOST_ASSERT(p);
        BOOST_ASSERT(in_);
        std::memcpy(p, in_, len);
        alloc_traits::deallocate(
            this->member(), p_, capacity());
    }
    p_ = p;
    in_ = p_;
    out_ = in_ + len;
    last_ = out_ + n;
    end_ = p_ + new_size;
    return {out_, n};
}

template<class Allocator>
void
basic_flat_buffer<Allocator>::
consume(std::size_t n)
{
    if(n >= dist(in_, out_))
    {
        in_ = p_;
        out_ = p_;
        return;
    }
    in_ += n;
}

template<class Allocator>
void
basic_flat_buffer<Allocator>::
reserve(std::size_t n)
{
    if(n <= capacity())
        return;
    if(n > max_)
        BOOST_THROW_EXCEPTION(std::length_error{
            "basic_flat_buffer overflow"});
    auto const new_size = (std::min)(max_,
        std::max<std::size_t>(
            detail::next_pow2(n), min_size));
    auto const p = alloc_traits::allocate(
        this->member(), new_size);
    auto const len = size();
    if(len > 0)
    {
        BOOST_ASSERT(p_);
        BOOST_ASSERT(in_);
        std::memcpy(p, in_, len);
        alloc_traits::deallocate(
            this->member(), p_, capacity());
    }
    p_ = p;
    in_ = p_;
    out_ = p_ + len;
    last_ = out_;
    end_ = p_ + new_size;
}

template<class Allocator>
void
basic_flat_buffer<Allocator>::
shrink_to_fit()
{
    auto const len = size();
    if(len == capacity())
        return;
    char* p;
    if(len > 0)
    {
        BOOST_ASSERT(p_);
        BOOST_ASSERT(in_);
        p = alloc_traits::allocate(
            this->member(), len);
        std::memcpy(p, in_, len);
    }
    else
    {
        p = nullptr;
    }
    alloc_traits::deallocate(
        this->member(), p_, dist(p_, end_));
    p_ = p;
    in_ = p_;
    out_ = p_ + len;
    last_ = out_;
    end_ = out_;
}

} // beast

#endif
