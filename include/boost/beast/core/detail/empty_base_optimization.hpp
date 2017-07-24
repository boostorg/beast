//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_DETAIL_EMPTY_BASE_OPTIMIZATION_HPP
#define BOOST_BEAST_DETAIL_EMPTY_BASE_OPTIMIZATION_HPP

#include <boost/type_traits/is_final.hpp>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast {
namespace detail {

template<class T>
struct empty_base_optimization_decide
    : std::integral_constant<bool,
        std::is_empty<T>::value &&
        ! boost::is_final<T>::value>
{
};

template<
    class T,
    int UniqueID = 0,
    bool ShouldDeriveFrom =
        empty_base_optimization_decide<T>::value
>
class empty_base_optimization : private T
{
public:
    empty_base_optimization() = default;

    empty_base_optimization(T const& t)
        : T (t)
    {}

    empty_base_optimization(T&& t)
        : T (std::move (t))
    {}

    T& member() noexcept
    {
        return *this;
    }

    T const& member() const noexcept
    {
        return *this;
    }
};

//------------------------------------------------------------------------------

template<
    class T,
    int UniqueID
>
class empty_base_optimization <T, UniqueID, false>
{
public:
    empty_base_optimization() = default;

    empty_base_optimization(T const& t)
        : m_t (t)
    {}

    empty_base_optimization(T&& t)
        : m_t (std::move (t))
    {}

    T& member() noexcept
    {
        return m_t;
    }

    T const& member() const noexcept
    {
        return m_t;
    }

private:
    T m_t;
};

} // detail
} // beast
} // boost

#endif
