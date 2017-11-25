//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_IMPL_BUFFERS_CAT_IPP
#define BOOST_BEAST_IMPL_BUFFERS_CAT_IPP

#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/throw_exception.hpp>
#include <array>
#include <cstdint>
#include <iterator>
#include <new>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace boost {
namespace beast {

template<class... Bn>
class buffers_cat_view<Bn...>::const_iterator
{
    static_assert(sizeof...(Bn) > 0,  "Must have at least 1 buffer sequence.");
    std::size_t n_;
    std::tuple<Bn...> const* bn_;
    detail::aligned_union_t<typename detail::buffer_sequence_iterator<
        Bn>::type...> buf_;

    friend class buffers_cat_view<Bn...>;

    template<std::size_t I>
    using C = std::integral_constant<std::size_t, I>;

    template<std::size_t I>
    using iter_t = typename detail::buffer_sequence_iterator<
        typename std::tuple_element<I, std::tuple<Bn...>>::type>::type;

    template<std::size_t I>
    iter_t<I>&
    iter()
    {
        BOOST_ASSERT(n_ ==  I);
        // type-pun
        return *reinterpret_cast<
            iter_t<I>*>(&buf_);
    }

    template<std::size_t I>
    iter_t<I> const&
    iter() const
    {
        BOOST_ASSERT(n_ ==  I);
        // type-pun
        return *reinterpret_cast<
            iter_t<I> const*>(&buf_);
    }

public:
    using value_type = typename
        detail::common_buffers_type<Bn...>::type;
    using pointer = value_type const*;
    using reference = value_type;
    using difference_type = std::ptrdiff_t;
    using iterator_category =
        std::bidirectional_iterator_tag;

    ~const_iterator();
    const_iterator();
    const_iterator(const_iterator&& other);
    const_iterator(const_iterator const& other);
    const_iterator& operator=(const_iterator&& other);
    const_iterator& operator=(const_iterator const& other);

    bool
    operator==(const_iterator const& other) const;

    bool
    operator!=(const_iterator const& other) const
    {
        return ! (*this == other);
    }

    reference
    operator*() const;

    pointer
    operator->() const = delete;

    const_iterator&
    operator++();

    const_iterator
    operator++(int);

    const_iterator&
    operator--();

    const_iterator
    operator--(int);

private:
    const_iterator(
        std::tuple<Bn...> const& bn, bool at_end);

    void
    construct(C<sizeof...(Bn)> const&)
    {
        auto constexpr I = sizeof...(Bn);
        n_ = I;
    }

    template<std::size_t I>
    void
    construct(C<I> const&)
    {
        if(boost::asio::buffer_size(
            std::get<I>(*bn_)) != 0)
        {
            n_ = I;
            new(&buf_) iter_t<I>{
                boost::asio::buffer_sequence_begin(
                std::get<I>(*bn_))};
            return;
        }
        construct(C<I+1>{});
    }

    void
    rconstruct(C<0> const&)
    {
        auto constexpr I = 0;
        if(boost::asio::buffer_size(
            std::get<I>(*bn_)) != 0)
        {
            n_ = I;
            new(&buf_) iter_t<I>{
                boost::asio::buffer_sequence_end(
                std::get<I>(*bn_))};
            return;
        }
        BOOST_THROW_EXCEPTION(std::logic_error{
            "invalid iterator"});
    }

    template<std::size_t I>
    void
    rconstruct(C<I> const&)
    {
        if(boost::asio::buffer_size(
            std::get<I>(*bn_)) != 0)
        {
            n_ = I;
            new(&buf_) iter_t<I>{
                boost::asio::buffer_sequence_end(
                    std::get<I>(*bn_))};
            return;
        }
        rconstruct(C<I-1>{});
    }

    void
    destroy(C<sizeof...(Bn)> const&)
    {
        return;
    }

    template<std::size_t I>
    void
    destroy(C<I> const&)
    {
        if(n_ == I)
        {
            using Iter = iter_t<I>;
            iter<I>().~Iter();
            return;
        }
        destroy(C<I+1>{});
    }

    void
    move(const_iterator&&,
        C<sizeof...(Bn)> const&)
    {
    }

    template<std::size_t I>
    void
    move(const_iterator&& other,
        C<I> const&)
    {
        if(n_ == I)
        {
            new(&buf_) iter_t<I>{
                std::move(other.iter<I>())};
            return;
        }
        move(std::move(other), C<I+1>{});
    }

    void
    copy(const_iterator const&,
        C<sizeof...(Bn)> const&)
    {
    }

    template<std::size_t I>
    void
    copy(const_iterator const& other,
        C<I> const&)
    {
        if(n_ == I)
        {
            new(&buf_) iter_t<I>{
                other.iter<I>()};
            return;
        }
        copy(other, C<I+1>{});
    }

    bool
    equal(const_iterator const&,
        C<sizeof...(Bn)> const&) const
    {
        return true;
    }

    template<std::size_t I>
    bool
    equal(const_iterator const& other,
        C<I> const&) const
    {
        if(n_ == I)
            return iter<I>() == other.iter<I>();
        return equal(other, C<I+1>{});
    }

    [[noreturn]]
    reference
    dereference(C<sizeof...(Bn)> const&) const
    {
        BOOST_THROW_EXCEPTION(std::logic_error{
            "invalid iterator"});
    }

    template<std::size_t I>
    reference
    dereference(C<I> const&) const
    {
        if(n_ == I)
            return *iter<I>();
        return dereference(C<I+1>{});
    }

    [[noreturn]]
    void
    increment(C<sizeof...(Bn)> const&)
    {
        BOOST_THROW_EXCEPTION(std::logic_error{
            "invalid iterator"});
    }

    template<std::size_t I>
    void
    increment(C<I> const&)
    {
        if(n_ == I)
        {
            if(++iter<I>() !=
                boost::asio::buffer_sequence_end(
                    std::get<I>(*bn_)))
                return;
            using Iter = iter_t<I>;
            iter<I>().~Iter();
            return construct(C<I+1>{});
        }
        increment(C<I+1>{});
    }

    void
    decrement(C<sizeof...(Bn)> const&)
    {
        auto constexpr I = sizeof...(Bn);
        if(n_ == I)
            rconstruct(C<I-1>{});
        decrement(C<I-1>{});
    }

    template<std::size_t I>
    void
    decrement(C<I> const&)
    {
        if(n_ == I)
        {
            if(iter<I>() !=
                boost::asio::buffer_sequence_begin(
                    std::get<I>(*bn_)))
            {
                --iter<I>();
                return;
            }
            using Iter = iter_t<I>;
            iter<I>().~Iter();
            --n_;
            rconstruct(C<I-1>{});
        }
        decrement(C<I-1>{});
    }

    void
    decrement(C<0> const&)
    {
        auto constexpr I = 0;
        if(iter<I>() !=
            boost::asio::buffer_sequence_begin(
                std::get<I>(*bn_)))
        {
            --iter<I>();
            return;
        }
        BOOST_THROW_EXCEPTION(std::logic_error{
            "invalid iterator"});
    }
};

//------------------------------------------------------------------------------

template<class... Bn>
buffers_cat_view<Bn...>::
const_iterator::~const_iterator()
{
    destroy(C<0>{});
}

template<class... Bn>
buffers_cat_view<Bn...>::
const_iterator::const_iterator()
    : n_(sizeof...(Bn))
    , bn_(nullptr)
{
}

template<class... Bn>
buffers_cat_view<Bn...>::
const_iterator::const_iterator(
    std::tuple<Bn...> const& bn, bool at_end)
    : bn_(&bn)
{
    if(at_end)
        n_ = sizeof...(Bn);
    else
        construct(C<0>{});
}

template<class... Bn>
buffers_cat_view<Bn...>::
const_iterator::const_iterator(const_iterator&& other)
    : n_(other.n_)
    , bn_(other.bn_)
{
    move(std::move(other), C<0>{});
}

template<class... Bn>
buffers_cat_view<Bn...>::
const_iterator::const_iterator(const_iterator const& other)
    : n_(other.n_)
    , bn_(other.bn_)
{
    copy(other, C<0>{});
}

template<class... Bn>
auto
buffers_cat_view<Bn...>::
const_iterator::operator=(const_iterator&& other) ->
    const_iterator&
{
    if(&other == this)
        return *this;
    destroy(C<0>{});
    n_ = other.n_;
    bn_ = other.bn_;
    // VFALCO What about exceptions?
    move(std::move(other), C<0>{});
    return *this;
}

template<class... Bn>
auto
buffers_cat_view<Bn...>::
const_iterator::operator=(const_iterator const& other) ->
const_iterator&
{
    if(&other == this)
        return *this;
    destroy(C<0>{});
    n_ = other.n_;
    bn_ = other.bn_;
    // VFALCO What about exceptions?
    copy(other, C<0>{});
    return *this;
}

template<class... Bn>
bool
buffers_cat_view<Bn...>::
const_iterator::operator==(const_iterator const& other) const
{
    if(bn_ != other.bn_)
        return false;
    if(n_ != other.n_)
        return false;
    return equal(other, C<0>{});
}

template<class... Bn>
auto
buffers_cat_view<Bn...>::
const_iterator::operator*() const ->
    reference
{
    return dereference(C<0>{});
}

template<class... Bn>
auto
buffers_cat_view<Bn...>::
const_iterator::operator++() ->
    const_iterator&
{
    increment(C<0>{});
    return *this;
}

template<class... Bn>
auto
buffers_cat_view<Bn...>::
const_iterator::operator++(int) ->
    const_iterator
{
    auto temp = *this;
    ++(*this);
    return temp;
}

template<class... Bn>
auto
buffers_cat_view<Bn...>::
const_iterator::operator--() ->
    const_iterator&
{
    decrement(C<sizeof...(Bn)>{});
    return *this;
}

template<class... Bn>
auto
buffers_cat_view<Bn...>::
const_iterator::operator--(int) ->
    const_iterator
{
    auto temp = *this;
    --(*this);
    return temp;
}

//------------------------------------------------------------------------------

template<class... Bn>
buffers_cat_view<Bn...>::
buffers_cat_view(Bn const&... bn)
    : bn_(bn...)
{
}


template<class... Bn>
inline
auto
buffers_cat_view<Bn...>::begin() const ->
    const_iterator
{
    return const_iterator{bn_, false};
}

template<class... Bn>
inline
auto
buffers_cat_view<Bn...>::end() const ->
    const_iterator
{
    return const_iterator{bn_, true};
}

} // beast
} // boost

#endif
