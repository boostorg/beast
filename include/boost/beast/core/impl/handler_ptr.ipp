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
    typename beast::detail::allocator_traits<
        boost::asio::associated_allocator_t<
            Handler>>::template rebind_alloc<T> alloc{
                boost::asio::get_associated_allocator(h_)};
    beast::detail::allocator_traits<
        decltype(alloc)>::destroy(alloc, t_);
    beast::detail::allocator_traits<
        decltype(alloc)>::deallocate(alloc, t_, 1);
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
    : t_([&]
        {
            BOOST_STATIC_ASSERT(! std::is_array<T>::value);
            typename beast::detail::allocator_traits<
                boost::asio::associated_allocator_t<
                    Handler>>::template rebind_alloc<T> alloc{
                        boost::asio::get_associated_allocator(handler)};
            using A = decltype(alloc);
            auto const d =
                [&alloc](T* p)
                {
                    beast::detail::allocator_traits<A>::deallocate(alloc, p, 1);
                };
            std::unique_ptr<T, decltype(d)> p{
                beast::detail::allocator_traits<A>::allocate(alloc, 1), d};
            beast::detail::allocator_traits<A>::construct(
                alloc, p.get(), handler, std::forward<Args>(args)...);
            return p.release();
        }())
    , h_(std::forward<DeducedHandler>(handler))
{
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
