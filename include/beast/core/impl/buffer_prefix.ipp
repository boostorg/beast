//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_IMPL_BUFFER_PREFIX_IPP
#define BEAST_IMPL_BUFFER_PREFIX_IPP

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace beast {

namespace detail {

inline
boost::asio::const_buffer
buffer_prefix(std::size_t size,
    boost::asio::const_buffer buffer)
{
    using boost::asio::buffer_cast;
    using boost::asio::buffer_size;
    return {buffer_cast<void const*>(buffer),
        (std::min)(size, buffer_size(buffer))};
}

inline
boost::asio::mutable_buffer
buffer_prefix(std::size_t size,
    boost::asio::mutable_buffer buffer)
{
    using boost::asio::buffer_cast;
    using boost::asio::buffer_size;
    return {buffer_cast<void*>(buffer),
        (std::min)(size, buffer_size(buffer))};
}

} // detail

template<class BufferSequence>
class buffer_prefix_view<BufferSequence>::const_iterator
{
    friend class buffer_prefix_view<BufferSequence>;

    buffer_prefix_view const* b_ = nullptr;
    std::size_t remain_;
    iter_type it_;

public:
    using value_type = typename std::conditional<
        std::is_convertible<typename
            std::iterator_traits<iter_type>::value_type,
                boost::asio::mutable_buffer>::value,
                    boost::asio::mutable_buffer,
                        boost::asio::const_buffer>::type;
    using pointer = value_type const*;
    using reference = value_type;
    using difference_type = std::ptrdiff_t;
    using iterator_category =
        std::bidirectional_iterator_tag;

    const_iterator() = default;
    const_iterator(const_iterator&& other) = default;
    const_iterator(const_iterator const& other) = default;
    const_iterator& operator=(const_iterator&& other) = default;
    const_iterator& operator=(const_iterator const& other) = default;

    bool
    operator==(const_iterator const& other) const
    {
        return b_ == other.b_ && it_ == other.it_;
    }

    bool
    operator!=(const_iterator const& other) const
    {
        return !(*this == other);
    }

    reference
    operator*() const
    {
        return detail::buffer_prefix(remain_, *it_);
    }

    pointer
    operator->() const = delete;

    const_iterator&
    operator++()
    {
        remain_ -= boost::asio::buffer_size(*it_++);
        return *this;
    }

    const_iterator
    operator++(int)
    {
        auto temp = *this;
        remain_ -= boost::asio::buffer_size(*it_++);
        return temp;
    }

    const_iterator&
    operator--()
    {
        remain_ += boost::asio::buffer_size(*--it_);
        return *this;
    }

    const_iterator
    operator--(int)
    {
        auto temp = *this;
        remain_ += boost::asio::buffer_size(*--it_);
        return temp;
    }

private:
    const_iterator(buffer_prefix_view const& b,
            std::true_type)
        : b_(&b)
        , remain_(0)
        , it_(b_->end_)
    {
    }

    const_iterator(buffer_prefix_view const& b,
            std::false_type)
        : b_(&b)
        , remain_(b_->size_)
        , it_(b_->bs_.begin())
    {
    }
};

template<class BufferSequence>
void
buffer_prefix_view<BufferSequence>::
setup(std::size_t size)
{
    size_ = 0;
    end_ = bs_.begin();
    auto const last = bs_.end();
    while(end_ != last)
    {
        auto const len =
            boost::asio::buffer_size(*end_++);
        if(len >= size)
        {
            size_ += size;
            break;
        }
        size -= len;
        size_ += len;
    }
}

template<class BufferSequence>
buffer_prefix_view<BufferSequence>::
buffer_prefix_view(buffer_prefix_view&& other)
    : buffer_prefix_view(std::move(other),
        std::distance<iter_type>(
            other.bs_.begin(), other.end_))
{
}

template<class BufferSequence>
buffer_prefix_view<BufferSequence>::
buffer_prefix_view(buffer_prefix_view const& other)
    : buffer_prefix_view(other,
        std::distance<iter_type>(
            other.bs_.begin(), other.end_))
{
}

template<class BufferSequence>
auto
buffer_prefix_view<BufferSequence>::
operator=(buffer_prefix_view&& other) ->
    buffer_prefix_view&
{
    auto const dist = std::distance<iter_type>(
        other.bs_.begin(), other.end_);
    bs_ = std::move(other.bs_);
    size_ = other.size_;
    end_ = std::next(bs_.begin(), dist);
    return *this;
}

template<class BufferSequence>
auto
buffer_prefix_view<BufferSequence>::
operator=(buffer_prefix_view const& other) ->
    buffer_prefix_view&
{
    auto const dist = std::distance<iter_type>(
        other.bs_.begin(), other.end_);
    bs_ = other.bs_;
    size_ = other.size_;
    end_ = std::next(bs_.begin(), dist);
    return *this;
}

template<class BufferSequence>
buffer_prefix_view<BufferSequence>::
buffer_prefix_view(std::size_t size,
        BufferSequence const& bs)
    : bs_(bs)
{
    setup(size);
}

template<class BufferSequence>
template<class... Args>
buffer_prefix_view<BufferSequence>::
buffer_prefix_view(std::size_t size,
        boost::in_place_init_t, Args&&... args)
    : bs_(std::forward<Args>(args)...)
{
    setup(size);
}

template<class BufferSequence>
inline
auto
buffer_prefix_view<BufferSequence>::begin() const ->
    const_iterator
{
    return const_iterator{*this, std::false_type{}};
}

template<class BufferSequence>
inline
auto
buffer_prefix_view<BufferSequence>::end() const ->
    const_iterator
{
    return const_iterator{*this, std::true_type{}};
}

} // beast

#endif
