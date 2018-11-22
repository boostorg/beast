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

#include <boost/beast/core/detail/lean_tuple.hpp>
#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/beast/core/detail/variant.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/throw_exception.hpp>
#include <cstdint>
#include <iterator>
#include <new>
#include <stdexcept>
#include <utility>

namespace boost {
namespace beast {

namespace detail {

struct buffers_cat_view_iterator_base
{
    struct past_end
    {
        char unused = 0; // make g++8 happy

        boost::asio::mutable_buffer
        operator*() const
        {
            BOOST_THROW_EXCEPTION(std::logic_error{
                "invalid iterator"});
        }

        operator bool() const noexcept
        {
            return true;
        }
    };
};

} // detail

template<class... Bn>
class buffers_cat_view<Bn...>::const_iterator
    : private detail::buffers_cat_view_iterator_base
{
    // VFALCO The logic to skip empty sequences fails
    //        if there is just one buffer in the list.
    static_assert(sizeof...(Bn) >= 2,
        "A minimum of two sequences are required");

    detail::lean_tuple<Bn...> const* bn_ = nullptr;
    detail::variant<typename
        detail::buffer_sequence_iterator<Bn>::type...,
            past_end> it_;

    friend class buffers_cat_view<Bn...>;

    template<std::size_t I>
    using C = std::integral_constant<std::size_t, I>;

public:
    using value_type = typename
        detail::common_buffers_type<Bn...>::type;
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

    // deprecated
    const_iterator&
    operator--();

    // deprecated
    const_iterator
    operator--(int);

private:
    const_iterator(
        detail::lean_tuple<Bn...> const& bn, bool at_end);

    template<std::size_t I>
    void
    next(C<I> const&)
    {
        if(boost::asio::buffer_size(
            detail::get<I>(*bn_)) != 0)
        {
            it_.template emplace<I+1>(
                boost::asio::buffer_sequence_begin(
                    detail::get<I>(*bn_)));
            return;
        }
        next(C<I+1>{});
    }

    void
    next(C<sizeof...(Bn)> const&)
    {
        // end
        auto constexpr I = sizeof...(Bn);
        it_.template emplace<I+1>();
    }

    template<std::size_t I>
    void
    prev(C<I> const&)
    {
        if(boost::asio::buffer_size(
            detail::get<I>(*bn_)) != 0)
        {
            it_.template emplace<I+1>(
                boost::asio::buffer_sequence_end(
                    detail::get<I>(*bn_)));
            return;
        }
        prev(C<I-1>{});
    }

    void
    prev(C<0> const&)
    {
        auto constexpr I = 0;
        it_.template emplace<I+1>(
            boost::asio::buffer_sequence_end(
                detail::get<I>(*bn_)));
    }

    struct dereference
    {
        const_iterator const& self;

        [[noreturn]]
        reference
        operator()(mp11::mp_size_t<0>)
        {
            BOOST_THROW_EXCEPTION(std::logic_error{
                "invalid iterator"});
        }

        template<class I>
        reference operator()(I)
        {
            return *self.it_.template get<I::value>();
        }
    };

    struct increment
    {
        const_iterator& self;

        [[noreturn]]
        void
        operator()(mp11::mp_size_t<0>)
        {
            BOOST_THROW_EXCEPTION(std::logic_error{
                "invalid iterator"});
        }

        [[noreturn]]
        void
        operator()(mp11::mp_size_t<sizeof...(Bn) + 1>)
        {
            (*this)(mp11::mp_size_t<0>{});
        }

        template<std::size_t I>
        void
        operator()(mp11::mp_size_t<I>)
        {
            auto& it = self.it_.template get<I>();
            if (++it == boost::asio::buffer_sequence_end(
                    detail::get<I - 1>(*self.bn_)))
                self.next(C<I>());
        }
    };

    void
    decrement(C<sizeof...(Bn)> const&)
    {
        auto constexpr I = sizeof...(Bn);
        if(it_.index() == I+1)
            prev(C<I-1>{});
        decrement(C<I-1>{});
    }

    template<std::size_t I>
    void
    decrement(C<I> const&)
    {
        if(it_.index() == I+1)
        {
            if(it_.template get<I+1>() !=
                boost::asio::buffer_sequence_begin(
                    detail::get<I>(*bn_)))
            {
                --it_.template get<I+1>();
                return;
            }
            prev(C<I-1>{});
        }
        decrement(C<I-1>{});
    }

    void
    decrement(C<0> const&)
    {
        auto constexpr I = 0;
        if(it_.template get<I+1>() !=
            boost::asio::buffer_sequence_begin(
                detail::get<I>(*bn_)))
        {
            --it_.template get<I+1>();
            return;
        }
        BOOST_THROW_EXCEPTION(std::logic_error{
            "invalid iterator"});
    }
};

//------------------------------------------------------------------------------

template<class... Bn>
buffers_cat_view<Bn...>::
const_iterator::
const_iterator(
    detail::lean_tuple<Bn...> const& bn, bool at_end)
    : bn_(&bn)
{
    if(! at_end)
        next(C<0>{});
    else
        next(C<sizeof...(Bn)>{});
}

template<class... Bn>
bool
buffers_cat_view<Bn...>::
const_iterator::
operator==(const_iterator const& other) const
{
    return
        (bn_ == nullptr) ?
        (
            other.bn_ == nullptr ||
            other.it_.index() == sizeof...(Bn)
        ):(
            (other.bn_ == nullptr) ?
            (
                it_.index() == sizeof...(Bn)
            ): (
                bn_ == other.bn_ &&
                it_ == other.it_
            )
        );
}

template<class... Bn>
auto
buffers_cat_view<Bn...>::
const_iterator::
operator*() const ->
    reference
{
    return mp11::mp_with_index<
        sizeof...(Bn) + 2>(
            it_.index(),
            dereference{*this});
}

template<class... Bn>
auto
buffers_cat_view<Bn...>::
const_iterator::
operator++() ->
    const_iterator&
{
    mp11::mp_with_index<
        sizeof...(Bn) + 2>(
            it_.index(),
            increment{*this});
    return *this;
}

template<class... Bn>
auto
buffers_cat_view<Bn...>::
const_iterator::
operator++(int) ->
    const_iterator
{
    auto temp = *this;
    ++(*this);
    return temp;
}

template<class... Bn>
auto
buffers_cat_view<Bn...>::
const_iterator::
operator--() ->
    const_iterator&
{
    decrement(C<sizeof...(Bn)>{});
    return *this;
}

template<class... Bn>
auto
buffers_cat_view<Bn...>::
const_iterator::
operator--(int) ->
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
