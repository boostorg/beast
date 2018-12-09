//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_IMPL_STATIC_BUFFER_HPP
#define BOOST_BEAST_IMPL_STATIC_BUFFER_HPP

#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/throw_exception.hpp>
#include <algorithm>
#include <cstring>
#include <iterator>
#include <stdexcept>

namespace boost {
namespace beast {

static_buffer_base::
static_buffer_base(
    void* p, std::size_t size) noexcept
    : begin_(static_cast<char*>(p))
    , capacity_(size)
{
}

auto
static_buffer_base::
data() const noexcept ->
    const_buffers_type
{
    using net::const_buffer;
    const_buffers_type result;
    if(in_off_ + in_size_ <= capacity_)
    {
        result[0] = const_buffer{begin_ + in_off_, in_size_};
        result[1] = const_buffer{begin_, 0};
    }
    else
    {
        result[0] = const_buffer{begin_ + in_off_, capacity_ - in_off_};
        result[1] = const_buffer{begin_, in_size_ - (capacity_ - in_off_)};
    }
    return result;
}

auto
static_buffer_base::
data() noexcept ->
    mutable_data_type
{
    using net::mutable_buffer;
    mutable_data_type result;
    if(in_off_ + in_size_ <= capacity_)
    {
        result[0] = mutable_buffer{begin_ + in_off_, in_size_};
        result[1] = mutable_buffer{begin_, 0};
    }
    else
    {
        result[0] = mutable_buffer{begin_ + in_off_, capacity_ - in_off_};
        result[1] = mutable_buffer{begin_, in_size_ - (capacity_ - in_off_)};
    }
    return result;
}

auto
static_buffer_base::
prepare(std::size_t n) ->
    mutable_buffers_type
{
    using net::mutable_buffer;
    if(n > capacity_ - in_size_)
        BOOST_THROW_EXCEPTION(std::length_error{
            "buffer overflow"});
    out_size_ = n;
    auto const out_off = (in_off_ + in_size_) % capacity_;
    mutable_buffers_type result;
    if(out_off + out_size_ <= capacity_ )
    {
        result[0] = mutable_buffer{begin_ + out_off, out_size_};
        result[1] = mutable_buffer{begin_, 0};
    }
    else
    {
        result[0] = mutable_buffer{begin_ + out_off, capacity_ - out_off};
        result[1] = mutable_buffer{begin_, out_size_ - (capacity_ - out_off)};
    }
    return result;
}

void
static_buffer_base::
commit(std::size_t n) noexcept
{
    in_size_ += (std::min)(n, out_size_);
    out_size_ = 0;
}

void
static_buffer_base::
consume(std::size_t n) noexcept
{
    if(n < in_size_)
    {
        in_off_ = (in_off_ + n) % capacity_;
        in_size_ -= n;
    }
    else
    {
        // rewind the offset, so the next call to prepare
        // can have a longer contiguous segment. this helps
        // algorithms optimized for larger buffers.
        in_off_ = 0;
        in_size_ = 0;
    }
}

void
static_buffer_base::
reset(void* p, std::size_t n) noexcept
{
    begin_ = static_cast<char*>(p);
    capacity_ = n;
    in_off_ = 0;
    in_size_ = 0;
    out_size_ = 0;
}

//------------------------------------------------------------------------------

template<std::size_t N>
static_buffer<N>::
static_buffer(static_buffer const& other) noexcept
    : static_buffer_base(buf_, N)
{
    this->commit(net::buffer_copy(
        this->prepare(other.size()), other.data()));
}

template<std::size_t N>
auto
static_buffer<N>::
operator=(static_buffer const& other) noexcept ->
    static_buffer<N>&
{
    this->consume(this->size());
    this->commit(net::buffer_copy(
        this->prepare(other.size()), other.data()));
    return *this;
}

} // beast
} // boost

#endif
