//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_IMPL_FLAT_STREAMBUF_HPP
#define BEAST_IMPL_FLAT_STREAMBUF_HPP

#include <boost/assert.hpp>
#include <stdexcept>

namespace beast {

/*  Memory is laid out thusly:

    p_ ..|.. in_ ..|.. out_ ..|.. last_ ..|.. end_
*/

template<class Allocator>
basic_flat_streambuf<Allocator>::
~basic_flat_streambuf()
{
    if(p_)
        alloc_traits::deallocate(
            this->member(), p_, dist(p_, end_));
}

template<class Allocator>
basic_flat_streambuf<Allocator>::
basic_flat_streambuf(basic_flat_streambuf&& other)
    : detail::empty_base_optimization<
        allocator_type>(std::move(other.member()))
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
basic_flat_streambuf<Allocator>::
basic_flat_streambuf(basic_flat_streambuf&& other,
        Allocator const& alloc)
    : detail::empty_base_optimization<
        allocator_type>(alloc)
{
    if(this->member() != other.member())
    {
        auto const n = other.size();
        p_ = alloc_traits::allocate(
            this->member(), n);
        in_ = p_;
        out_ = p_ + n;
        last_ = out_;
        end_ = out_;
        max_ = other.max_;
        std::memcpy(in_, other.in_, n);
        return;
    }
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
basic_flat_streambuf<Allocator>::
basic_flat_streambuf(
        basic_flat_streambuf const& other)
    : detail::empty_base_optimization<allocator_type>(
        alloc_traits::select_on_container_copy_construction(
            other.member()))
{
    auto const n = other.size();
    p_ = alloc_traits::allocate(
        this->member(), n);
    in_ = p_;
    out_ = p_ + n;
    last_ = out_;
    end_ = out_;
    max_ = other.max_;
    std::memcpy(in_, other.in_, n);
}

template<class Allocator>
basic_flat_streambuf<Allocator>::
basic_flat_streambuf(
    basic_flat_streambuf const& other,
        Allocator const& alloc)
    : detail::empty_base_optimization<
        allocator_type>(alloc)
{
    auto const n = other.size();
    p_ = alloc_traits::allocate(
        this->member(), n);
    in_ = p_;
    out_ = p_ + n;
    last_ = out_;
    end_ = out_;
    max_ = other.max_;
    std::memcpy(in_, other.in_, n);
}
    
template<class Allocator>
template<class OtherAlloc>
basic_flat_streambuf<Allocator>::
basic_flat_streambuf(
    basic_flat_streambuf<OtherAlloc> const& other)
{
    auto const n = other.size();
    p_ = alloc_traits::allocate(
        this->member(), n);
    in_ = p_;
    out_  = p_ + n;
    last_ = out_;
    end_ = out_;
    max_ = other.max_;
    std::memcpy(in_, other.in_, n);
}

template<class Allocator>
template<class OtherAlloc>
basic_flat_streambuf<Allocator>::
basic_flat_streambuf(
    basic_flat_streambuf<OtherAlloc> const& other,
        Allocator const& alloc)
    : detail::empty_base_optimization<
        allocator_type>(alloc)
{
    auto const n = other.size();
    p_ = alloc_traits::allocate(
        this->member(), n);
    in_ = p_;
    out_  = p_ + n;
    last_ = out_;
    end_ = out_;
    max_ = other.max_;
    std::memcpy(in_, other.in_, n);
}

template<class Allocator>
basic_flat_streambuf<Allocator>::
basic_flat_streambuf(std::size_t limit)
    : p_(nullptr)
    , in_(nullptr)
    , out_(nullptr)
    , last_(nullptr)
    , end_(nullptr)
    , max_(limit)
{
}

template<class Allocator>
basic_flat_streambuf<Allocator>::
basic_flat_streambuf(
    Allocator const& alloc, std::size_t limit)
    : detail::empty_base_optimization<
        allocator_type>(alloc)
    , p_(nullptr)
    , in_(nullptr)
    , out_(nullptr)
    , last_(nullptr)
    , end_(nullptr)
    , max_(limit)
{
}

template<class Allocator>
auto
basic_flat_streambuf<Allocator>::
prepare(std::size_t n) ->
    mutable_buffers_type
{
    if(n <= dist(out_, end_))
    {
        last_ = out_ + n;
        return{out_, n};
    }
    auto const len = size();
    if(n <= dist(p_, end_) - len)
    {
        std::memmove(p_, in_, len);
        in_ = p_;
        out_ = in_ + len;
        last_ = out_ + n;
        return {out_, n};
    }
    if(n > max_ - len)
        throw std::length_error{
            "flat_streambuf overflow"};
    auto const p = alloc_traits::allocate(
        this->member(), len + n);
    if(len > 0)
        std::memcpy(p, in_, len);
    alloc_traits::deallocate(
        this->member(), p_, dist(p_, end_));
    p_ = p;
    in_ = p_;
    out_ = in_ + len;
    last_ = out_ + n;
    end_ = last_;
    return {out_, n};
}

template<class Allocator>
void
basic_flat_streambuf<Allocator>::
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
basic_flat_streambuf<Allocator>::
reserve(std::size_t n)
{
    if(n <= dist(p_, end_))
        return;
    if(n > max_)
        throw std::length_error{
            "flat_streambuf overflow"};
    auto const p = alloc_traits::allocate(
        this->member(), n);
    auto const len = size();
    std::memcpy(p, in_, len);
    alloc_traits::deallocate(
        this->member(), p_, dist(p_, end_));
    p_ = p;
    in_ = p_;
    out_ = p_ + len;
    last_ = out_;
    end_ = p + n;
}

template<class Allocator>
void
basic_flat_streambuf<Allocator>::
shrink_to_fit()
{
    auto const len = size();
    if(len == dist(p_, end_))
        return;
    auto const p = alloc_traits::allocate(
        this->member(), len);
    std::memcpy(p, in_, len);
    alloc_traits::deallocate(
        this->member(), p_, dist(p_, end_));
    p_ = p;
    in_ = p_;
    out_ = p_ + len;
    last_ = out_;
    end_ = out_;
}

template<class Allocator>
std::size_t
read_size_helper(basic_flat_streambuf<
    Allocator> const& fb, std::size_t max_size)
{
    // If this goes off it means you forgot to
    // call reserve() before using the buffer.
    BOOST_ASSERT(fb.capacity() != 0);
    BOOST_ASSERT(max_size >= 1);
    auto const len = fb.size();
    auto const avail = fb.capacity() - len;
    if (avail > 0)
        return (std::min)(avail, max_size);
    auto size = (std::min)(
        fb.capacity() * 2, fb.max_size()) - len;
    if(size == 0)
        size = 1;
    return (std::min)(size, max_size);
}

} // beast

#endif
