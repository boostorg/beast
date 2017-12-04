//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_IMPL_HANDLER_PTR_HPP
#define BOOST_BEAST_IMPL_HANDLER_PTR_HPP

#include <boost/asio/associated_allocator.hpp>
#include <boost/assert.hpp>
#include <memory>

namespace boost {
namespace beast {

template<class T, class Handler>
void
handler_ptr<T, Handler>::
clear()
{
    using alloc_t = typename detail::allocator_traits<
    boost::asio::associated_allocator_t<Handler>>::template rebind_alloc<T>;
    using alloc_traits_t = detail::allocator_traits<alloc_t>;

    alloc_t alloc{boost::asio::get_associated_allocator(h_)};
    alloc_traits_t::destroy(alloc, t_);
    alloc_traits_t::deallocate(alloc, t_, 1);
    t_ = nullptr;
}

template<class T, class Handler>
handler_ptr<T, Handler>::
~handler_ptr()
{
    if(t_)
        clear();
}

template<class T, class Handler>
handler_ptr<T, Handler>::
handler_ptr(handler_ptr&& other)
    : t_(other.t_)
    , h_(std::move(other.h_))
{
    other.t_ = nullptr;
}

template<class T, class Handler>
template<class DeducedHandler, class... Args>
handler_ptr<T, Handler>::
handler_ptr(DeducedHandler&& handler, Args&&... args)
    : h_(std::forward<DeducedHandler>(handler))
{
    BOOST_STATIC_ASSERT(! std::is_array<T>::value);
    using alloc_t = typename detail::allocator_traits<
        boost::asio::associated_allocator_t<Handler>>::template rebind_alloc<T>;
    using alloc_traits_t = detail::allocator_traits<alloc_t>;

    alloc_t alloc{boost::asio::get_associated_allocator(h_)};
    t_ = alloc_traits_t::allocate(alloc, 1);
    try
    {
        alloc_traits_t::construct(alloc, t_, h_, std::forward<Args>(args)...);
    }
    catch(...)
    {
        alloc_traits_t::deallocate(alloc, t_, 1);
        throw;
    }
}

template<class T, class Handler>
auto
handler_ptr<T, Handler>::
release_handler() ->
    handler_type
{
    BOOST_ASSERT(t_);
    clear();
    return std::move(h_);
}

template<class T, class Handler>
template<class... Args>
void
handler_ptr<T, Handler>::
invoke(Args&&... args)
{
    BOOST_ASSERT(t_);
    clear();
    h_(std::forward<Args>(args)...);
}

} // beast
} // boost

#endif
