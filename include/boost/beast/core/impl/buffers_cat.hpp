//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_IMPL_BUFFERS_CAT_HPP
#define BOOST_BEAST_IMPL_BUFFERS_CAT_HPP

#include <boost/beast/core/detail/tuple.hpp>
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

        net::mutable_buffer
        operator*() const
        {
            // Dereferencing a one-past-the-end iterator
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

    detail::tuple<Bn...> const* bn_ = nullptr;
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
    const_iterator(const_iterator const& other) = default;
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

    const_iterator&
    operator--();

    const_iterator
    operator--(int);

private:
    const_iterator(
        detail::tuple<Bn...> const& bn, bool at_end);

    struct dereference
    {
        const_iterator const& self;

        [[noreturn]]
        reference
        operator()(mp11::mp_size_t<0>)
        {
            // Dereferencing a default-constructed iterator
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
            // Incrementing a default-constructed iterator
            BOOST_THROW_EXCEPTION(std::logic_error{
                "invalid iterator"});
        }

        template<std::size_t I>
        void
        operator()(mp11::mp_size_t<I>)
        {
            ++self.it_.template get<I>();
            next(mp11::mp_size_t<I>{});
        }

        template<std::size_t I>
        void
        next(mp11::mp_size_t<I>)
        {
            auto& it = self.it_.template get<I>();
            for(;;)
            {
                if (it == net::buffer_sequence_end(
                        detail::get<I-1>(*self.bn_)))
                    break;
                if(net::const_buffer(*it).size() > 0)
                    return;
                ++it;
            }
            self.it_.template emplace<I+1>(
                net::buffer_sequence_begin(
                    detail::get<I>(*self.bn_)));
            next(mp11::mp_size_t<I+1>{});
        }

        void
        operator()(mp11::mp_size_t<sizeof...(Bn)>)
        {
            auto constexpr I = sizeof...(Bn);
            ++self.it_.template get<I>();
            next(mp11::mp_size_t<I>{});
        }

        void
        next(mp11::mp_size_t<sizeof...(Bn)>)
        {
            auto constexpr I = sizeof...(Bn);
            auto& it = self.it_.template get<I>();
            for(;;)
            {
                if (it == net::buffer_sequence_end(
                        detail::get<I-1>(*self.bn_)))
                    break;
                if(net::const_buffer(*it).size() > 0)
                    return;
                ++it;
            }
            // end
            self.it_.template emplace<I+1>();
        }

        [[noreturn]]
        void
        operator()(mp11::mp_size_t<sizeof...(Bn)+1>)
        {
            // Incrementing a one-past-the-end iterator
            BOOST_THROW_EXCEPTION(std::logic_error{
                "invalid iterator"});
        }
    };

    struct decrement
    {
        const_iterator& self;

        [[noreturn]]
        void
        operator()(mp11::mp_size_t<0>)
        {
            // Decrementing a default-constructed iterator
            BOOST_THROW_EXCEPTION(std::logic_error{
                "invalid iterator"});
        }

        void
        operator()(mp11::mp_size_t<1>)
        {
            auto constexpr I = 1;

            auto& it = self.it_.template get<I>();
            for(;;)
            {
                if(it == net::buffer_sequence_begin(
                        detail::get<I-1>(*self.bn_)))
                {
                    // Decrementing an iterator to the beginning
                    BOOST_THROW_EXCEPTION(std::logic_error{
                        "invalid iterator"});
                }
                --it;
                if(net::const_buffer(*it).size() > 0)
                    return;
            }
        }

        template<std::size_t I>
        void
        operator()(mp11::mp_size_t<I>)
        {
            auto& it = self.it_.template get<I>();
            for(;;)
            {
                if(it == net::buffer_sequence_begin(
                        detail::get<I-1>(*self.bn_)))
                    break;
                --it;
                if(net::const_buffer(*it).size() > 0)
                    return;
            }
            self.it_.template emplace<I-1>(
                net::buffer_sequence_end(
                    detail::get<I-2>(*self.bn_)));
            (*this)(mp11::mp_size_t<I-1>{});
        }

        void
        operator()(mp11::mp_size_t<sizeof...(Bn)+1>)
        {
            auto constexpr I = sizeof...(Bn)+1;
            self.it_.template emplace<I-1>(
                net::buffer_sequence_end(
                    detail::get<I-2>(*self.bn_)));
            (*this)(mp11::mp_size_t<I-1>{});
        }
    };
};

//------------------------------------------------------------------------------

template<class... Bn>
buffers_cat_view<Bn...>::
const_iterator::
const_iterator(
    detail::tuple<Bn...> const& bn, bool at_end)
    : bn_(&bn)
{
    if(! at_end)
    {
        it_.template emplace<1>(
            net::buffer_sequence_begin(
                detail::get<0>(*bn_)));
        increment{*this}.next(
            mp11::mp_size_t<1>{});
    }
    else
    {
        it_.template emplace<sizeof...(Bn)+1>();
    }
}

template<class... Bn>
bool
buffers_cat_view<Bn...>::
const_iterator::
operator==(const_iterator const& other) const
{
    return bn_ == other.bn_ && it_ == other.it_;
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
    mp11::mp_with_index<
        sizeof...(Bn) + 2>(
            it_.index(),
            decrement{*this});
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
auto
buffers_cat_view<Bn...>::begin() const ->
    const_iterator
{
    return const_iterator{bn_, false};
}

template<class... Bn>
auto
buffers_cat_view<Bn...>::end() const->
    const_iterator
{
    return const_iterator{bn_, true};
}

} // beast
} // boost

#endif
