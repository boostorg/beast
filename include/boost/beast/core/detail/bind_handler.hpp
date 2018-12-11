//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_DETAIL_BIND_HANDLER_HPP
#define BOOST_BEAST_DETAIL_BIND_HANDLER_HPP

#include <boost/beast/core/error.hpp>
#include <boost/beast/core/detail/tuple.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/handler_alloc_hook.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/mp11/integer_sequence.hpp>
#include <boost/is_placeholder.hpp>
#include <functional>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast {
namespace detail {

template<class Handler, class... Args>
class bind_wrapper
{
    using args_type = detail::tuple<
        typename std::decay<Args>::type...>;

    Handler h_;
    args_type args_;

    template<class T, class Executor>
    friend struct net::associated_executor;

    template<class Arg, class Vals>
    static
    typename std::enable_if<
        std::is_placeholder<typename
            std::decay<Arg>::type>::value == 0 &&
        boost::is_placeholder<typename
            std::decay<Arg>::type>::value == 0,
        Arg&&>::type
    extract(Arg&& arg, Vals&& vals)
    {
        boost::ignore_unused(vals);
        return std::forward<Arg>(arg);
    }

    template<class Arg, class Vals>
    static
    typename std::enable_if<
        std::is_placeholder<typename
            std::decay<Arg>::type>::value != 0,
        tuple_element<std::is_placeholder<
            typename std::decay<Arg>::type>::value - 1,
        Vals>>::type&&
    extract(Arg&&, Vals&& vals)
    {
        return detail::get<std::is_placeholder<
            typename std::decay<Arg>::type>::value - 1>(
                std::forward<Vals>(vals));
    }

    template<class Arg, class Vals>
    static
    typename std::enable_if<
        boost::is_placeholder<typename
            std::decay<Arg>::type>::value != 0,
        tuple_element<boost::is_placeholder<
            typename std::decay<Arg>::type>::value - 1,
        Vals>>::type&&
    extract(Arg&&, Vals&& vals)
    {
        return detail::get<boost::is_placeholder<
            typename std::decay<Arg>::type>::value - 1>(
                std::forward<Vals>(vals));
    }

    template<
        class ArgsTuple,
        std::size_t... S>
    static
    void
    invoke(
        Handler& h,
        ArgsTuple& args,
        tuple<>&&,
        mp11::index_sequence<S...>)
    {
        boost::ignore_unused(args);
        h(detail::get<S>(std::move(args))...);
    }

    template<
        class ArgsTuple,
        class ValsTuple,
        std::size_t... S>
    static
    void
    invoke(
        Handler& h,
        ArgsTuple& args,
        ValsTuple&& vals,
        mp11::index_sequence<S...>)
    {
        boost::ignore_unused(args);
        boost::ignore_unused(vals);
        h(extract(detail::get<S>(std::move(args)),
            std::forward<ValsTuple>(vals))...);
    }

public:
    using result_type = void;

    bind_wrapper(bind_wrapper&&) = default;
    bind_wrapper(bind_wrapper const&) = default;

    template<class DeducedHandler>
    explicit
    bind_wrapper(
            DeducedHandler&& handler, Args&&... args)
        : h_(std::forward<DeducedHandler>(handler))
        , args_(std::forward<Args>(args)...)
    {
    }

    template<class... Values>
    void
    operator()(Values&&... values)
    {
        invoke(h_, args_,
            tuple<Values&&...>(
                std::forward<Values>(values)...),
            mp11::index_sequence_for<Args...>());
    }

    // VFALCO I don't think this should be needed,
    //        please let me know if something breaks.
    /*
    template<class... Values>
    void
    operator()(Values&&... values) const
    {
        invoke(h_, args_,
            tuple<Values&&...>(
                std::forward<Values>(values)...),
            mp11::index_sequence_for<Args...>());
    }
    */

    //

    using allocator_type =
        net::associated_allocator_t<Handler>;

    allocator_type
    get_allocator() const noexcept
    {
        return net::get_associated_allocator(h_);
    }

    template<class Function>
    friend
    void asio_handler_invoke(
        Function&& f, bind_wrapper* w)
    {
        using net::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(w->h_));
    }

    friend
    bool asio_handler_is_continuation(
        bind_wrapper* op)
    {
        using net::asio_handler_is_continuation;
        return asio_handler_is_continuation(
                std::addressof(op->h_));
    }
};

template<class Handler, class... Args>
class bind_front_wrapper;

//------------------------------------------------------------------------------

// 0-arg specialization
template<class Handler>
class bind_front_wrapper<Handler>
{
    Handler h_;

    template<class T, class Executor>
    friend struct net::associated_executor;

public:
    using result_type = void;

    bind_front_wrapper(bind_front_wrapper&&) = default;
    bind_front_wrapper(bind_front_wrapper const&) = default;

    template<class Handler_>
    explicit
    bind_front_wrapper(Handler_&& handler)
        : h_(std::forward<Handler_>(handler))
    {
    }

    template<class... Ts>
    void operator()(Ts&&... ts)
    {
        h_(std::forward<Ts>(ts)...);
    }

    //

    using allocator_type =
        net::associated_allocator_t<Handler>;

    allocator_type
    get_allocator() const noexcept
    {
        return net::get_associated_allocator(h_);
    }

    template<class Function>
    friend
    void asio_handler_invoke(
        Function&& f, bind_front_wrapper* w)
    {
        using net::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(w->h_));
    }

    friend
    bool asio_handler_is_continuation(
        bind_front_wrapper* op)
    {
        using net::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->h_));
    }
};

//------------------------------------------------------------------------------

// 1-arg specialization
template<class Handler, class Arg>
class bind_front_wrapper<Handler, Arg>
{
    Handler h_;
    Arg arg_;

    template<class T, class Executor>
    friend struct net::associated_executor;

public:
    using result_type = void;

    bind_front_wrapper(bind_front_wrapper&&) = default;
    bind_front_wrapper(bind_front_wrapper const&) = default;

    template<
        class Handler_,
        class Arg_>
    bind_front_wrapper(
        Handler_&& handler,
        Arg_&& arg)
        : h_(std::forward<Handler_>(handler))
        , arg_(std::forward<Arg_>(arg))
    {
    }

    template<class... Ts>
    void operator()(Ts&&... ts)
    {
        h_( std::move(arg_),
            std::forward<Ts>(ts)...);
    }

    //

    using allocator_type =
        net::associated_allocator_t<Handler>;

    allocator_type
    get_allocator() const noexcept
    {
        return net::get_associated_allocator(h_);
    }

    template<class Function>
    friend
    void asio_handler_invoke(
        Function&& f, bind_front_wrapper* w)
    {
        using net::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(w->h_));
    }

    friend
    bool asio_handler_is_continuation(
        bind_front_wrapper* op)
    {
        using net::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->h_));
    }
};

//------------------------------------------------------------------------------

// 2-arg specialization
template<class Handler, class Arg1, class Arg2>
class bind_front_wrapper<Handler, Arg1, Arg2>
{
    Handler h_;
    Arg1 arg1_;
    Arg2 arg2_;

    template<class T, class Executor>
    friend struct net::associated_executor;

public:
    using result_type = void;

    bind_front_wrapper(bind_front_wrapper&&) = default;
    bind_front_wrapper(bind_front_wrapper const&) = default;

    template<
        class Handler_,
        class Arg1_,
        class Arg2_>
    bind_front_wrapper(
        Handler_&& handler,
        Arg1_&& arg1,
        Arg2_&& arg2)
        : h_(std::forward<Handler_>(handler))
        , arg1_(std::forward<Arg1_>(arg1))
        , arg2_(std::forward<Arg2_>(arg2))
    {
    }

    template<class... Ts>
    void operator()(Ts&&... ts)
    {
        h_( std::move(arg1_),
            std::move(arg2_),
            std::forward<Ts>(ts)...);
    }

    //

    using allocator_type =
        net::associated_allocator_t<Handler>;

    allocator_type
    get_allocator() const noexcept
    {
        return net::get_associated_allocator(h_);
    }

    template<class Function>
    friend
    void asio_handler_invoke(
        Function&& f, bind_front_wrapper* w)
    {
        using net::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(w->h_));
    }

    friend
    bool asio_handler_is_continuation(
        bind_front_wrapper* op)
    {
        using net::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->h_));
    }

};

//------------------------------------------------------------------------------

// 3+ arg specialization
template<
    class Handler,
    class Arg1, class Arg2, class Arg3,
    class... Args>
class bind_front_wrapper<
    Handler, Arg1, Arg2, Arg3, Args...>
{
    Handler h_;
    detail::tuple<
        Arg1, Arg2, Arg3, Args...> args_;

    template<class T, class Executor>
    friend struct net::associated_executor;

    template<std::size_t... I, class... Ts>
    void
    invoke(
        mp11::index_sequence<I...>,
        Ts&&... ts)
    {
        h_( detail::get<I>(std::move(args_))...,
            std::forward<Ts>(ts)...);
    }

public:
    using result_type = void;

    bind_front_wrapper(bind_front_wrapper&&) = default;
    bind_front_wrapper(bind_front_wrapper const&) = default;

    template<
        class Handler_,
        class Arg1_, class Arg2_, class Arg3_,
        class... Args_>
    bind_front_wrapper(
        Handler_&& handler,
        Arg1_&& arg1,
        Arg2_&& arg2,
        Arg3_&& arg3,
        Args_&&... args)
        : h_(std::forward<Handler_>(handler))
        , args_(
            std::forward<Arg1_>(arg1),
            std::forward<Arg2_>(arg2),
            std::forward<Arg3_>(arg3),
            std::forward<Args_>(args)...)
    {
    }

    template<class... Ts>
    void operator()(Ts&&... ts)
    {
        invoke(
            mp11::index_sequence_for<
                Arg1, Arg2, Arg3, Args...>{},
            std::forward<Ts>(ts)...);
    }

    //

    using allocator_type =
        net::associated_allocator_t<Handler>;

    allocator_type
    get_allocator() const noexcept
    {
        return net::get_associated_allocator(h_);
    }

    template<class Function>
    friend
    void asio_handler_invoke(
        Function&& f, bind_front_wrapper* w)
    {
        using net::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(w->h_));
    }

    friend
    bool asio_handler_is_continuation(
        bind_front_wrapper* op)
    {
        using net::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->h_));
    }
};

//------------------------------------------------------------------------------

// specialization for the most common case,
// to reduce instantiation time and memory.
template<class Handler>
class bind_front_wrapper<
    Handler, error_code, std::size_t>
{
    template<class T, class Executor>
    friend struct net::associated_executor;

    Handler h_;
    error_code ec_;
    std::size_t n_;

public:
    using result_type = void;

    bind_front_wrapper(bind_front_wrapper&&) = default;
    bind_front_wrapper(bind_front_wrapper const&) = default;

    template<class DeducedHandler>
    bind_front_wrapper(DeducedHandler&& handler,
        error_code ec, std::size_t n)
        : h_(std::forward<DeducedHandler>(handler))
        , ec_(ec)
        , n_(n)
    {
    }

    template<class... Ts>
    void operator()(Ts&&... ts)
    {
        h_(ec_, n_, std::forward<Ts>(ts)...);
    }

    //

    using allocator_type =
        net::associated_allocator_t<Handler>;

    allocator_type
    get_allocator() const noexcept
    {
        return net::get_associated_allocator(h_);
    }

    template<class Function>
    friend
    void asio_handler_invoke(
        Function&& f, bind_front_wrapper* w)
    {
        using net::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(w->h_));
    }

    friend
    bool asio_handler_is_continuation(
        bind_front_wrapper* op)
    {
        using net::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->h_));
    }
};

//------------------------------------------------------------------------------

// VFALCO These are only called when using deprecated
// network interfaces in conjunction with extensions,
// and should not be needed. If this creates a problem
// please contact me.

template<class Handler, class... Args>
void* asio_handler_allocate(std::size_t,
    bind_wrapper<Handler, Args...>* op) = delete;

template<class Handler, class... Args>
void asio_handler_deallocate(void*, std::size_t,
    bind_wrapper<Handler, Args...>* op) = delete;

template<class Handler, class... Args>
void* asio_handler_allocate(std::size_t,
    bind_front_wrapper<Handler, Args...>* op) = delete;

template<class Handler, class... Args>
void asio_handler_deallocate(void*, std::size_t,
    bind_front_wrapper<Handler, Args...>* op) = delete;

} // detail
} // beast
} // boost

//------------------------------------------------------------------------------

namespace boost {
namespace asio {

template<class Handler, class... Args, class Executor>
struct associated_executor<
    beast::detail::bind_wrapper<Handler, Args...>, Executor>
{
    using type = typename
        associated_executor<Handler, Executor>::type;

    static
    type
    get(beast::detail::bind_wrapper<Handler, Args...> const& w,
        Executor const& ex = Executor()) noexcept
    {
        return associated_executor<
            Handler, Executor>::get(w.h_, ex);
    }
};

template<class Handler, class... Args, class Executor>
struct associated_executor<
    beast::detail::bind_front_wrapper<Handler, Args...>, Executor>
{
    using type = typename
        associated_executor<Handler, Executor>::type;

    static
    type
    get(beast::detail::bind_front_wrapper<Handler, Args...> const& w,
        Executor const& ex = Executor()) noexcept
    {
        return associated_executor<
            Handler, Executor>::get(w.h_, ex);
    }
};

} // asio
} // boost

//------------------------------------------------------------------------------

namespace std {

// VFALCO Using std::bind on a completion handler will
// cause undefined behavior later, because the executor
// associated with the handler is not propagated to the
// wrapper returned by std::bind; these overloads are
// deleted to prevent mistakes. If this creates a problem
// please contact me.

template<class Handler, class... Args>
void
bind(boost::beast::detail::bind_wrapper<
    Handler, Args...>, ...) = delete;

template<class Handler, class... Args>
void
bind(boost::beast::detail::bind_front_wrapper<
    Handler, Args...>, ...) = delete;

} // std

//------------------------------------------------------------------------------

#endif
