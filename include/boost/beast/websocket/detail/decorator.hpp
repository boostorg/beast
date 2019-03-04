//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_DETAIL_DECORATOR_HPP
#define BOOST_BEAST_WEBSOCKET_DETAIL_DECORATOR_HPP

#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/core/exchange.hpp>
#include <boost/type_traits/make_void.hpp>
#include <algorithm>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast {
namespace websocket {
namespace detail {

class decorator
{
    friend class decorator_test;

    struct incomplete;

    struct exemplar
    {
        void (incomplete::*mf)();
        std::shared_ptr<incomplete> sp;
        void* param;
    };

    union storage
    {
        void* p_;
        void (*fn_)();
        alignas(alignof(exemplar)) char buf_[sizeof(exemplar)];
    };

    struct vtable
    {
        void (*move_construct)(storage& dst, storage& src) noexcept;
        void (*destroy)(storage& dst) noexcept;
        void (*invoke_req)(storage& dst, request_type& req);
        void (*invoke_resp)(storage& dst, response_type& req);

        static vtable const* get_default()
        {
            static const vtable impl{
                [](storage&, storage&) noexcept {},
                [](storage&) noexcept {},
                [](storage&, request_type&){},
                [](storage&, response_type&){},
            };
            return &impl;
        }
    };

    template<class F,
             bool Inline = (sizeof(F) <= sizeof(storage) &&
                            alignof(F) <= alignof(storage) &&
                            std::is_nothrow_move_constructible<F>::value)>
    struct vtable_impl;


    storage storage_;
    vtable const* vtable_ = vtable::get_default();

    // VFALCO NOTE: When this is two traits, one for
    //              request and one for response,
    //              Visual Studio 2015 fails.

    template<class T, class U, class = void>
    struct is_op_of : std::false_type
    {
    };

    template<class T, class U>
    struct is_op_of<T, U, boost::void_t<decltype(
        std::declval<T&>()(std::declval<U&>()))>>
      : std::true_type
    {
    };

    template <class F, class U>
    static void try_invoke(F& f, U& u, std::true_type)
    {
        f(u);
    }

    template <class F, class U>
    static void try_invoke(F& f, U& u, std::false_type)
    {
    }

public:
    decorator(decorator const&) = delete;
    decorator& operator=(decorator const&) = delete;

    ~decorator()
    {
        vtable_->destroy(storage_);
    }

    decorator() = default;

    decorator(decorator&& other) noexcept
        : vtable_(boost::exchange(other.vtable_, vtable::get_default()))
    {
        vtable_->move_construct(storage_, other.storage_);
    }

    template<class F,
             class = typename std::enable_if<
               !std::is_convertible<F, decorator>::value>::type>
    explicit decorator(F&& f)
      : vtable_(vtable_impl<typename std::decay<F>::type>::
        construct(storage_, std::forward<F>(f)))
    {
    }

    decorator& operator=(decorator&& other) noexcept
    {
        vtable_->destroy(storage_);
        vtable_ = boost::exchange(other.vtable_, vtable::get_default());
        vtable_->move_construct(storage_, other.storage_);
        return *this;
    }

    void operator()(request_type& req)
    {
        vtable_->invoke_req(storage_, req);
    }

    void operator()(response_type& res)
    {
        vtable_->invoke_resp(storage_, res);
    }
};

template<class F>
struct decorator::vtable_impl<F, true>
{
    template<class Arg>
    static vtable const* construct(storage& dst, Arg&& arg)
    {
        ::new (static_cast<void*>(&dst.buf_)) F(std::forward<Arg>(arg));
        return get();
    }

    static void move_construct(storage& dst, storage& src) noexcept
    {
        auto& f = *reinterpret_cast<F*>(&src.buf_);
        ::new (static_cast<void*>(&dst.buf_)) F(std::move(f));
    }

    static void destroy(storage& dst) noexcept
    {
        reinterpret_cast<F*>(&dst.buf_)->~F();
    }

    static void invoke_req(storage& dst, request_type& req)
    {
        auto& f = *reinterpret_cast<F*>(&dst.buf_);
        try_invoke(f, req, is_op_of<F, request_type>{});
    }

    static void invoke_resp(storage& dst, response_type& req)
    {
        auto& f = *reinterpret_cast<F*>(&dst.buf_);
        try_invoke(f, req, is_op_of<F, response_type>{});
    }

    static vtable const* get()
    {
        static constexpr vtable impl{&move_construct,
            &destroy, &invoke_req, &invoke_resp};
        return &impl;
    }
};

template<class F>
struct decorator::vtable_impl<F, false>
{
    template<class Arg>
    static vtable const* construct(storage& dst, Arg&& arg)
    {
        dst.p_ = new F(std::forward<Arg>(arg));
        return get();
    }

    static void move_construct(storage& dst, storage& src) noexcept
    {
        dst.p_ = src.p_;
    }

    static void destroy(storage& dst) noexcept
    {
        delete static_cast<F*>(dst.p_);
    }

    static void invoke_req(storage& dst, request_type& req)
    {
        auto& f = *static_cast<F*>(dst.p_);
        try_invoke(f, req, is_op_of<F, request_type>{});
    }

    static void invoke_resp(storage& dst, response_type& req)
    {
        auto& f = *static_cast<F*>(dst.p_);
        try_invoke(f, req, is_op_of<F, response_type>{});
    }

    static vtable const* get()
    {
        static constexpr vtable impl{&move_construct,
            &destroy, &invoke_req, &invoke_resp};
        return &impl;
    }
};

} // detail
} // websocket
} // beast
} // boost

#endif
