//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_IMPL_STATIC_BUFFER_IPP
#define BOOST_BEAST_IMPL_STATIC_BUFFER_IPP

#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/throw_exception.hpp>
#include <algorithm>
#include <cstring>
#include <iterator>
#include <stdexcept>

namespace boost {
namespace beast {

inline
static_buffer_base::
static_buffer_base(void* p, std::size_t size)
    : begin_(reinterpret_cast<char*>(p))
    , capacity_(size)
{
}

inline
auto
static_buffer_base::
data() const ->
    const_buffers_type
{
    using boost::asio::const_buffer;
    if(in_off_ + in_size_ <= capacity_)
        return {
            const_buffer{ begin_ + in_off_, in_size_ },
            const_buffer{ begin_, 0 } };
    return {
        const_buffer{ begin_ + in_off_, capacity_ - in_off_ },
        const_buffer{ begin_, in_size_ - (capacity_ - in_off_) } };
}

inline
auto
static_buffer_base::
mutable_data() ->
    mutable_data_type
{
    using boost::asio::mutable_buffer;
    if(in_off_ + in_size_ <= capacity_)
        return {
            mutable_buffer{ begin_ + in_off_, in_size_ },
            mutable_buffer{ begin_, 0 } };
    return {
        mutable_buffer{ begin_ + in_off_, capacity_ - in_off_ },
        mutable_buffer{ begin_, in_size_ - (capacity_ - in_off_) } };
}

inline
auto
static_buffer_base::
prepare(std::size_t size) ->
    mutable_buffers_type
{
    using boost::asio::mutable_buffer;
    if(size > capacity_ - in_size_)
        BOOST_THROW_EXCEPTION(std::length_error{
            "buffer overflow"});
    out_size_ = size;
    auto const out_off = (in_off_ + in_size_) % capacity_;
    if(out_off + out_size_ <= capacity_ )
        return {
            mutable_buffer{ begin_ + out_off, out_size_ },
            mutable_buffer{ begin_, 0 } };
    return {
        mutable_buffer{ begin_ + out_off, capacity_ - out_off },
        mutable_buffer{ begin_, out_size_ - (capacity_ - out_off) } };
}

inline
void
static_buffer_base::
commit(std::size_t size)
{
    in_size_ += (std::min)(size, out_size_);
    out_size_ = 0;
}

inline
void
static_buffer_base::
consume(std::size_t size)
{
    if(size < in_size_)
    {
        in_off_ = (in_off_ + size) % capacity_;
        in_size_ -= size;
    }
    else
    {
        // rewind the offset, so the next call to prepare
        // can have a longer continguous segment. this helps
        // algorithms optimized for larger buffesr.
        in_off_ = 0;
        in_size_ = 0;
    }
}

inline
void
static_buffer_base::
reset(void* p, std::size_t size)
{
    begin_ = reinterpret_cast<char*>(p);
    capacity_ = size;
    in_off_ = 0;
    in_size_ = 0;
    out_size_ = 0;
}

//------------------------------------------------------------------------------

template<std::size_t N>
static_buffer<N>::
static_buffer(static_buffer const& other)
    : static_buffer_base(buf_, N)
{
    using boost::asio::buffer_copy;
    this->commit(buffer_copy(
        this->prepare(other.size()), other.data()));
}

template<std::size_t N>
auto
static_buffer<N>::
operator=(static_buffer const& other) ->
    static_buffer<N>&
{
    using boost::asio::buffer_copy;
    this->consume(this->size());
    this->commit(buffer_copy(
        this->prepare(other.size()), other.data()));
    return *this;
}

} // beast
} // boost

#endif
