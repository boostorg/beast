//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_DETAIL_VARIANT_HPP
#define BOOST_BEAST_DETAIL_VARIANT_HPP

#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/assert.hpp>
#include <cstddef>
#include <tuple>
#include <type_traits>

namespace boost {
namespace beast {
namespace detail {

// This simple variant gets the job done without
// causing too much trouble with template depth:
//
// * Always allows an empty state I==0
// * emplace() and get() support 1-based indexes only
// * Basic exception guarantee
// * Max 255 types
//
template<class... TN>
class variant
{
    typename std::aligned_storage<
        max_sizeof<TN...>(),
        max_alignof<TN...>()
    >::type buf_;
    unsigned char i_ = 0;

    template<std::size_t I>
    using type = typename std::tuple_element<
        I , std::tuple<TN...>>::type;

    template<std::size_t I>
    using C = std::integral_constant<std::size_t, I>;

public:
    variant() = default;

    ~variant()
    {
        if(i_)
            destroy(C<0>{});
    }

    // 0 = empty
    unsigned char
    index() const
    {
        return i_;
    }

    // moved-from object becomes empty
    variant(variant&& other)
    {
        i_ = other.move(&buf_, C<0>{});
    }

    variant(variant const& other)
    {
        i_ = other.copy(&buf_, C<0>{});
    }

    // moved-from object becomes empty
    variant& operator=(variant&& other)
    {
        if(i_ != 0)
            destroy(C<0>{});
        i_ = other.move(&buf_, C<0>{});
        return *this;
    }

    variant& operator=(variant const& other)
    {
        if(i_ != 0)
            destroy(C<0>{});
        i_ = other.copy(&buf_, C<0>{});
        return *this;
    }

    template<std::size_t I, class... Args>
    void
    emplace(Args&&... args)
    {
        if(i_ != 0)
            destroy(C<0>{});
        new(&buf_) type<I-1>(
            std::forward<Args>(args)...);
        i_ = I;
    }

    template<std::size_t I>
    type<I-1>&
    get()
    {
        BOOST_ASSERT(i_ == I);
        return *reinterpret_cast<
            type<I-1>*>(&buf_);
    }

    template<std::size_t I>
    type<I-1> const&
    get() const
    {
        BOOST_ASSERT(i_ == I);
        return *reinterpret_cast<
            type<I-1> const*>(&buf_);
    }

    void
    reset()
    {
        if(i_ == 0)
            return;
        destroy(C<0>{});
    }

private:
    void
    destroy(C<sizeof...(TN)>)
    {
        return;
    }

    template<std::size_t I>
    void
    destroy(C<I>)
    {
        if(i_ == I+1)
        {
            using T = type<I>;
            get<I+1>().~T();
            i_ = 0;
            return;
        }
        destroy(C<I+1>{});
    }

    unsigned char
    move(void*, C<sizeof...(TN)>)
    {
        return 0;
    }

    template<std::size_t I>
    unsigned char
    move(void* dest, C<I>)
    {
        if(i_ == I+1)
        {
            using T = type<I>;
            new(dest) T{std::move(get<I+1>())};
            get<I+1>().~T();
            i_ = 0;
            return I+1;
        }
        return move(dest, C<I+1>{});
    }

    unsigned char
    copy(void*, C<sizeof...(TN)>) const
    {
        return 0;
    }

    template<std::size_t I>
    unsigned char
    copy(void* dest, C<I>) const
    {
        if(i_ == I+1)
        {
            using T = type<I>;
            auto const& t = get<I+1>();
            new(dest) T{t};
            return I+1;
        }
        return copy(dest, C<I+1>{});
    }
};

} // detail
} // beast
} // boost

#endif
