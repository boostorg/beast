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
#include <boost/beast/core/detail/integer_sequence.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/mp11/integer_sequence.hpp>
#include <boost/is_placeholder.hpp>
#include <functional>
#include <utility>

namespace boost {
namespace beast {
namespace detail {

template<class Handler, class... Args>
class bind_wrapper
{
    using args_type = std::tuple<
        typename std::decay<Args>::type...>;

    Handler h_;
    args_type args_;

    // Can't friend partial specializations,
    // so we just friend the whole thing.
    template<class T, class Executor>
    friend struct boost::asio::associated_executor;

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
        typename std::tuple_element<
            std::is_placeholder<
                typename std::decay<Arg>::type>::value - 1,
        Vals>>::type::type&&
    extract(Arg&&, Vals&& vals)
    {
        return std::get<std::is_placeholder<
            typename std::decay<Arg>::type>::value - 1>(
                std::forward<Vals>(vals));
    }

    template<class Arg, class Vals>
    static
    typename std::enable_if<
        boost::is_placeholder<typename
            std::decay<Arg>::type>::value != 0,
        typename std::tuple_element<
            boost::is_placeholder<
                typename std::decay<Arg>::type>::value - 1,
        Vals>>::type::type&&
    extract(Arg&&, Vals&& vals)
    {
        return std::get<boost::is_placeholder<
            typename std::decay<Arg>::type>::value - 1>(
                std::forward<Vals>(vals));
    }

    template<
        class ArgsTuple,
        std::size_t... S>
    static
    void
    invoke(
        Handler& w,
        ArgsTuple& args,
        std::tuple<>&&,
        index_sequence<S...>)
    {
        boost::ignore_unused(args);
        w(std::get<S>(std::move(args))...);
    }

    template<
        class ArgsTuple,
        class ValsTuple,
        std::size_t... S>
    static
    void
    invoke(
        Handler& w,
        ArgsTuple& args,
        ValsTuple&& vals,
        index_sequence<S...>)
    {
        boost::ignore_unused(args);
        boost::ignore_unused(vals);
        w(extract(std::get<S>(std::move(args)),
            std::forward<ValsTuple>(vals))...);
    }

public:
    using result_type = void;

    using allocator_type =
        boost::asio::associated_allocator_t<Handler>;

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

    allocator_type
    get_allocator() const noexcept
    {
        return (boost::asio::get_associated_allocator)(h_);
    }

    friend
    bool
    asio_handler_is_continuation(bind_wrapper* w)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(std::addressof(w->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, bind_wrapper* w)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(w->h_));
    }

    template<class... Values>
    void
    operator()(Values&&... values)
    {
        invoke(h_, args_,
            std::forward_as_tuple(
                std::forward<Values>(values)...),
            index_sequence_for<Args...>());
    }

    template<class... Values>
    void
    operator()(Values&&... values) const
    {
        invoke(h_, args_,
            std::forward_as_tuple(
                std::forward<Values>(values)...),
            index_sequence_for<Args...>());
    }
};

//------------------------------------------------------------------------------

template<class Handler, class... Args>
class bind_front_wrapper;

// 0-arg specialization
template<class Handler>
class bind_front_wrapper<Handler>
{
    Handler h_;

    // Can't friend partial specializations,
    // so we just friend the whole thing.
    template<class T, class Executor>
    friend struct boost::asio::associated_executor;

public:
    using result_type = void;

    using allocator_type =
        boost::asio::associated_allocator_t<Handler>;

    bind_front_wrapper(bind_front_wrapper&&) = default;
    bind_front_wrapper(bind_front_wrapper const&) = default;

    template<class DeducedHandler>
    explicit
    bind_front_wrapper(DeducedHandler&& handler)
        : h_(std::forward<DeducedHandler>(handler))
    {
    }

    allocator_type
    get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(h_);
    }

    friend
    bool
    asio_handler_is_continuation(bind_front_wrapper* w)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(std::addressof(w->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, bind_front_wrapper* w)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(w->h_));
    }

    template<class... Ts>
    void operator()(Ts&&... ts)
    {
        h_(std::forward<Ts>(ts)...);
    }
};

// 1-arg specialization
template<class Handler, class Arg>
class bind_front_wrapper<Handler, Arg>
{
    Handler h_;
    typename std::decay<Arg>::type arg_;

    // Can't friend partial specializations,
    // so we just friend the whole thing.
    template<class T, class Executor>
    friend struct boost::asio::associated_executor;

public:
    using result_type = void;

    using allocator_type =
        boost::asio::associated_allocator_t<Handler>;

    bind_front_wrapper(bind_front_wrapper&&) = default;
    bind_front_wrapper(bind_front_wrapper const&) = default;

    template<class DeducedHandler>
    bind_front_wrapper(
        DeducedHandler&& handler, Arg&& arg)
        : h_(std::forward<DeducedHandler>(handler))
        , arg_(std::forward<Arg>(arg))
    {
    }

    allocator_type
    get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(h_);
    }

    friend
    bool
    asio_handler_is_continuation(bind_front_wrapper* w)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(std::addressof(w->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, bind_front_wrapper* w)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(w->h_));
    }

    template<class... Ts>
    void operator()(Ts&&... ts)
    {
        h_( std::forward<Arg>(arg_),
            std::forward<Ts>(ts)...);
    }
};

// 2-arg specialization
template<class Handler, class Arg1, class Arg2>
class bind_front_wrapper<Handler, Arg1, Arg2>
{
    Handler h_;
    typename std::decay<Arg1>::type arg1_;
    typename std::decay<Arg2>::type arg2_;

    // Can't friend partial specializations,
    // so we just friend the whole thing.
    template<class T, class Executor>
    friend struct boost::asio::associated_executor;

public:
    using result_type = void;

    using allocator_type =
        boost::asio::associated_allocator_t<Handler>;

    bind_front_wrapper(bind_front_wrapper&&) = default;
    bind_front_wrapper(bind_front_wrapper const&) = default;

    template<class DeducedHandler>
    bind_front_wrapper(DeducedHandler&& handler,
            Arg1&& arg1, Arg2&& arg2)
        : h_(std::forward<DeducedHandler>(handler))
        , arg1_(std::forward<Arg1>(arg1))
        , arg2_(std::forward<Arg2>(arg2))
    {
    }

    allocator_type
    get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(h_);
    }

    friend
    bool
    asio_handler_is_continuation(bind_front_wrapper* w)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(std::addressof(w->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, bind_front_wrapper* w)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(w->h_));
    }

    template<class... Ts>
    void operator()(Ts&&... ts)
    {
        h_( std::forward<Arg1>(arg1_),
            std::forward<Arg2>(arg2_),
            std::forward<Ts>(ts)...);
    }
};

// 3+ arg specialization
template<class Handler,
    class Arg1, class Arg2, class Arg3, class... Args>
class bind_front_wrapper<Handler, Arg1, Arg2, Arg3, Args...>
{
    using args_type = std::tuple<
        typename std::decay<Arg1>::type,
        typename std::decay<Arg2>::type,
        typename std::decay<Arg3>::type,
        typename std::decay<Args>::type...>;

    Handler h_;
    args_type args_;

    // Can't friend partial specializations,
    // so we just friend the whole thing.
    template<class T, class Executor>
    friend struct boost::asio::associated_executor;

    template<std::size_t... I, class... Ts>
    void
    invoke(
        boost::mp11::index_sequence<I...>,
        Ts&&... ts)
    {
        h_( std::get<I>(std::move(args_))...,
            std::forward<Ts>(ts)...);
    }

public:
    using result_type = void;

    using allocator_type =
        boost::asio::associated_allocator_t<Handler>;

    bind_front_wrapper(bind_front_wrapper&&) = default;
    bind_front_wrapper(bind_front_wrapper const&) = default;

    template<class DeducedHandler>
    bind_front_wrapper(DeducedHandler&& handler,
        Arg1&& arg1, Arg2&& arg2, Arg3&& arg3,
            Args&&... args)
        : h_(std::forward<DeducedHandler>(handler))
        , args_(
            std::forward<Arg1>(arg1),
            std::forward<Arg2>(arg2),
            std::forward<Arg3>(arg3),
            std::forward<Args>(args)...)
    {
    }

    allocator_type
    get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(h_);
    }

    friend
    bool
    asio_handler_is_continuation(bind_front_wrapper* w)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(std::addressof(w->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, bind_front_wrapper* w)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(w->h_));
    }

    template<class... Ts>
    void operator()(Ts&&... ts)
    {
        invoke(
            boost::mp11::index_sequence_for<
                Arg1, Arg2, Arg3, Args...>{},
            std::forward<Ts>(ts)...);
    }
};

//------------------------------------------------------------------------------

// specialization for the most common case,
// to reduce instantiation time and memory.
template<class Handler>
class bind_front_wrapper<
    Handler, error_code, std::size_t>
{
    Handler h_;
    error_code ec_;
    std::size_t n_;

public:
    using result_type = void;

    using allocator_type =
        boost::asio::associated_allocator_t<Handler>;

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

    allocator_type
    get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(h_);
    }

    friend
    bool
    asio_handler_is_continuation(bind_front_wrapper* w)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(std::addressof(w->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, bind_front_wrapper* w)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(w->h_));
    }

    void operator()()
    {
        h_(ec_, n_);
    }
};

} // detail
} // beast

//------------------------------------------------------------------------------

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

namespace std {

template<class Handler, class... Args>
void
bind(boost::beast::detail::bind_wrapper<
    Handler, Args...>, ...) = delete;

template<class Handler, class... Args>
void
bind(boost::beast::detail::bind_front_wrapper<
    Handler, Args...>, ...) = delete;

} // std

#endif
