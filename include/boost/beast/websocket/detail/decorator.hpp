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
#include <boost/core/empty_value.hpp>
#include <boost/core/exchange.hpp>
#include <boost/type_traits/make_void.hpp>
#include <algorithm>
#include <functional>
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
    struct incomplete;

    static std::size_t constexpr Bytes =
        beast::detail::max_sizeof<
            void*,
            void const*,
            void(*)(),
            void(incomplete::*)(),
            decltype(std::bind(
                std::declval<
                    void(incomplete::*)(request_type&)>(),
                std::shared_ptr<incomplete>{},
                std::placeholders::_1))
        >();

    struct base
    {
        virtual ~base() = default;

        virtual
        base*
        move(void* to) = 0;

        virtual
        void
        invoke(request_type&) = 0;

        virtual
        void
        invoke(response_type&) = 0;
    };

    using type = typename
        std::aligned_storage<Bytes>::type;

    type buf_;
    base* base_;

    struct none
    {
    };

    template<class T, class = void>
    struct is_req_op : std::false_type
    {
    };

    template<class T>
    struct is_req_op<T, boost::void_t<decltype(
        std::declval<T&>()(std::declval<request_type&>())
        )>> : std::true_type
    {
    };

    template<class T, class = void>
    struct is_res_op : std::false_type
    {
    };

    template<class T>
    struct is_res_op<T, boost::void_t<decltype(
        std::declval<T&>()(std::declval<response_type&>())
        )>> : std::true_type
    {
    };

    template<class F>
    struct impl : base,
        boost::empty_value<F>
    {
        impl(impl&&) = default;

        template<class F_>
        explicit
        impl(F_&& f)
            : boost::empty_value<F>(
                boost::empty_init_t{},
                std::forward<F_>(f))
        {
        }

        base*
        move(void* to) override
        {
            return ::new(to) impl(
                std::move(this->get()));
        }

        void
        invoke(request_type& req) override
        {
            this->invoke(req, is_req_op<F>{});
        }

        void
        invoke(request_type& req, std::true_type)
        {
            this->get()(req);
        }

        void
        invoke(request_type&, std::false_type)
        {
        }

        void
        invoke(response_type& res) override
        {
            this->invoke(res, is_res_op<F>{});
        }

        void
        invoke(response_type& res, std::true_type)
        {
            this->get()(res);
        }

        void
        invoke(response_type&, std::false_type)
        {
        }
    };

    void
    destroy()
    {
        if(is_inline())
            base_->~base();
        else if(base_)
            delete base_;
    }

    template<class F>
    base*
    construct(F&& f, std::true_type)
    {
        return ::new(&buf_) impl<
            typename std::decay<F>::type>(
                std::forward<F>(f));
    }

    template<class F>
    base*
    construct(F&& f, std::false_type)
    {
        return new impl<
            typename std::decay<F>::type>(
                std::forward<F>(f));
    }

public:
    decorator(decorator const&) = delete;
    decorator& operator=(decorator const&) = delete;

    ~decorator()
    {
        destroy();
    }

    decorator()
        : decorator(none{})
    {
    }

    decorator(
        decorator&& other)
    {
        if(other.is_inline())
        {
            base_ = other.base_->move(&buf_);
            other.base_->~base();
        }
        else
        {
            base_ = other.base_;
        }
        other.base_ = ::new(&other.buf_)
            impl<none>(none{});
    }

    template<class F,
        class = typename std::enable_if<
        ! std::is_convertible<F, decorator>::value>::type>
    explicit
    decorator(F&& f)
        : base_(construct(std::forward<F>(f),
            std::integral_constant<bool,
                sizeof(F) <= sizeof(buf_)>{}))
    {
    }

    decorator&
    operator=(decorator&& other)
    {
        this->destroy();
        base_ = nullptr;
        if(other.is_inline())
        {
            base_ = other.base_->move(&buf_);
            other.base_->~base();
        }
        else
        {
            base_ = other.base_;
        }
        other.base_ = ::new(&other.buf_)
            impl<none>(none{});
        return *this;
    }

    bool
    is_inline() const noexcept
    {
        return
            ! std::less<void const*>()(
                reinterpret_cast<void const*>(base_),
                reinterpret_cast<void const*>(&buf_)) &&
            std::less<void const*>()(
                reinterpret_cast<void const*>(base_),
                reinterpret_cast<void const*>(&buf_ + 1));
    }

    void
    operator()(request_type& req)
    {
        base_->invoke(req);
    }

    void
    operator()(response_type& res)
    {
        base_->invoke(res);
    }
};

} // detail
} // websocket
} // beast
} // boost

#endif
