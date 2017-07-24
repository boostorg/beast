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

template<bool isInput, class T>
class static_buffer_base::buffers_type
{
public:
    using value_type = T;
    class const_iterator
    {
        friend class buffers_type;

        static_buffer_base const* b_ = nullptr;
        int seg_ = 0;

        const_iterator(bool at_end,
            static_buffer_base const& b)
            : b_(&b)
        {
            if(! at_end)
            {
                seg_ = 0;
            }
            else
            {
                set_end(std::integral_constant<bool, isInput>{});
            }
        }

        void
        set_end(std::true_type)
        {
            if(b_->in_off_ + b_->in_size_ <= b_->capacity_)
                seg_ = 1;
            else
                seg_ = 2;
        }

        void
        set_end(std::false_type)
        {
            if(((b_->in_off_ + b_->in_size_) % b_->capacity_)
                    + b_->out_size_ <= b_->capacity_)
                seg_ = 1;
            else
                seg_ = 2;
        }

        T
        dereference(std::true_type) const
        {
            switch(seg_)
            {
            case 0:
                return {
                    b_->begin_ + b_->in_off_,
                    (std::min)(
                        b_->in_size_,
                        b_->capacity_ - b_->in_off_)};
            default:
            case 1:
                return {
                    b_->begin_,
                    b_->in_size_ - (
                        b_->capacity_ - b_->in_off_)};
            }
        }

        T
        dereference(std::false_type) const
        {
            switch(seg_)
            {
            case 0:
            {
                auto const out_off =
                    (b_->in_off_ + b_->in_size_)
                        % b_->capacity_;
                return {
                    b_->begin_ + out_off,
                    (std::min)(
                        b_->out_size_,
                        b_->capacity_ - out_off)};
            }
            default:
            case 1:
            {
                auto const out_off =
                    (b_->in_off_ + b_->in_size_)
                        % b_->capacity_;
                return {
                    b_->begin_,
                    b_->out_size_ - (
                        b_->capacity_ - out_off)};
                break;
            }
            }
        }

    public:
        using value_type = T;
        using pointer = value_type const*;
        using reference = value_type const&;
        using difference_type = std::ptrdiff_t;
        using iterator_category =
            std::bidirectional_iterator_tag;

        const_iterator() = default;
        const_iterator(const_iterator const& other) = default;
        const_iterator& operator=(const_iterator const& other) = default;

        bool
        operator==(const_iterator const& other) const
        {
            return b_ == other.b_ && seg_ == other.seg_;
        }

        bool
        operator!=(const_iterator const& other) const
        {
            return !(*this == other);
        }

        value_type
        operator*() const
        {
            return dereference(
                std::integral_constant<bool, isInput>{});
        }

        pointer
        operator->() = delete;

        const_iterator&
        operator++()
        {
            ++seg_;
            return *this;
        }

        const_iterator
        operator++(int)
        {
            auto temp = *this;
            ++(*this);
            return temp;
        }

        const_iterator&
        operator--()
        {
            --seg_;
            return *this;
        }

        const_iterator
        operator--(int)
        {
            auto temp = *this;
            --(*this);
            return temp;
        }
    };

    buffers_type() = delete;
    buffers_type(buffers_type const&) = default;
    buffers_type& operator=(buffers_type const&) = delete;

    const_iterator
    begin() const
    {
        return const_iterator{false, b_};
    }

    const_iterator
    end() const
    {
        return const_iterator{true, b_};
    }

private:
    friend class static_buffer_base;

    static_buffer_base const& b_;

    explicit
    buffers_type(static_buffer_base const& b)
        : b_(b)
    {
    }
};

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
    return const_buffers_type{*this};
}

inline
auto
static_buffer_base::
mutable_data() ->
    mutable_data_type
{
    return mutable_data_type{*this};
}

inline
auto
static_buffer_base::
prepare(std::size_t size) ->
    mutable_buffers_type
{
    if(size > capacity_ - in_size_)
        BOOST_THROW_EXCEPTION(std::length_error{
            "buffer overflow"});
    out_size_ = size;
    return mutable_buffers_type{*this};
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
