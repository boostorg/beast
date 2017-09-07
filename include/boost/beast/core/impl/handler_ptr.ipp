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
template<class DeducedHandler, class... Args>
inline
handler_ptr<T, Handler>::P::
P(DeducedHandler&& h, Args&&... args)
    : n(1)
    , handler(std::forward<DeducedHandler>(h))
{
    typename std::allocator_traits<
        boost::asio::associated_allocator_t<Handler>>::
            template rebind_alloc<T> alloc{
                boost::asio::get_associated_allocator(handler)};
    t = std::allocator_traits<decltype(alloc)>::allocate(alloc, 1);
    try
    {
        t = new(t) T{handler,
            std::forward<Args>(args)...};
    }
    catch(...)
    {
        std::allocator_traits<
            decltype(alloc)>::deallocate(alloc, t, 1);
        throw;
    }
}

template<class T, class Handler>
handler_ptr<T, Handler>::
~handler_ptr()
{
    if(! p_)
        return;
    if(--p_->n)
        return;
    if(p_->t)
    {
        p_->t->~T();
        typename std::allocator_traits<
            boost::asio::associated_allocator_t<Handler>>::
                template rebind_alloc<T> alloc{
                    boost::asio::get_associated_allocator(
                        p_->handler)};
        std::allocator_traits<
            decltype(alloc)>::deallocate(alloc, p_->t, 1);
    }
    delete p_;
}

template<class T, class Handler>
handler_ptr<T, Handler>::
handler_ptr(handler_ptr&& other)
    : p_(other.p_)
{
    other.p_ = nullptr;
}

template<class T, class Handler>
handler_ptr<T, Handler>::
handler_ptr(handler_ptr const& other)
    : p_(other.p_)
{
    if(p_)
        ++p_->n;
}

template<class T, class Handler>
template<class... Args>
handler_ptr<T, Handler>::
handler_ptr(Handler&& handler, Args&&... args)
    : p_(new P{std::move(handler),
        std::forward<Args>(args)...})
{
    BOOST_STATIC_ASSERT(! std::is_array<T>::value);
}

template<class T, class Handler>
template<class... Args>
handler_ptr<T, Handler>::
handler_ptr(Handler const& handler, Args&&... args)
    : p_(new P{handler, std::forward<Args>(args)...})
{
    BOOST_STATIC_ASSERT(! std::is_array<T>::value);
}

template<class T, class Handler>
auto
handler_ptr<T, Handler>::
release_handler() ->
    handler_type
{
    BOOST_ASSERT(p_);
    BOOST_ASSERT(p_->t);
    p_->t->~T();
    typename std::allocator_traits<
        boost::asio::associated_allocator_t<Handler>>::
            template rebind_alloc<T> alloc{
                boost::asio::get_associated_allocator(
                    p_->handler)};
    std::allocator_traits<
        decltype(alloc)>::deallocate(alloc, p_->t, 1);
    p_->t = nullptr;
    return std::move(p_->handler);
}

template<class T, class Handler>
template<class... Args>
void
handler_ptr<T, Handler>::
invoke(Args&&... args)
{
    BOOST_ASSERT(p_);
    BOOST_ASSERT(p_->t);
    p_->t->~T();
    typename std::allocator_traits<
        boost::asio::associated_allocator_t<Handler>>::
            template rebind_alloc<T> alloc{
                boost::asio::get_associated_allocator(
                    p_->handler)};
    std::allocator_traits<
        decltype(alloc)>::deallocate(alloc, p_->t, 1);
    p_->t = nullptr;
    p_->handler(std::forward<Args>(args)...);
}

} // beast
} // boost

#endif
