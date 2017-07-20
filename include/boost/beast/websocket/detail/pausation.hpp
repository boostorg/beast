//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_DETAIL_PAUSATION_HPP
#define BOOST_BEAST_WEBSOCKET_DETAIL_PAUSATION_HPP

#include <boost/beast/core/handler_ptr.hpp>
#include <boost/assert.hpp>
#include <array>
#include <memory>
#include <new>
#include <utility>

namespace boost {
namespace beast {
namespace websocket {
namespace detail {

// A container that holds a suspended, asynchronous composed
// operation. The contained object may be invoked later to
// resume the operation, or the container may be destroyed.
//
class pausation
{
    struct base
    {
        base() = default;
        base(base &&) = default;
        virtual ~base() = default;
        virtual base* move(void* p) = 0;
        virtual void operator()() = 0;
    };

    template<class F>
    struct holder : base
    {
        F f;

        holder(holder&&) = default;

        template<class U>
        explicit
        holder(U&& u)
            : f(std::forward<U>(u))
        {
        }

        base*
        move(void* p) override
        {
            return ::new(p) holder(std::move(*this));
        }

        void
        operator()() override
        {
            F f_(std::move(f));
            this->~holder();
            // invocation of f_() can
            // assign a new object to *this.
            f_();
        }
    };

    struct exemplar
    {
        struct H
        {
            void operator()();
        };

        struct T
        {
            using handler_type = H;
        };

        handler_ptr<T, H> hp;

        void operator()();
    };

    template<class Op>
    class saved_op
    {
        Op* op_ = nullptr;

    public:
        ~saved_op()
        {
            using boost::asio::asio_handler_deallocate;
            if(op_)
            {
                auto h = std::move(op_->handler());
                op_->~Op();
                asio_handler_deallocate(op_,
                    sizeof(*op_), std::addressof(h));
            }
        }

        saved_op(saved_op&& other)
            : op_(other.op_)
        {
            other.op_ = nullptr;
        }

        saved_op& operator=(saved_op&& other)
        {
            BOOST_ASSERT(! op_);
            op_ = other.op_;
            other.op_ = 0;
            return *this;
        }

        explicit
        saved_op(Op&& op)
        {
            using boost::asio::asio_handler_allocate;
            new(asio_handler_allocate(sizeof(Op),
                std::addressof(op.handler()))) Op{
                    std::move(op)};
        }

        void
        operator()()
        {
            BOOST_ASSERT(op_);
            Op op{std::move(*op_)};
            using boost::asio::asio_handler_deallocate;
            asio_handler_deallocate(op_,
                sizeof(*op_), std::addressof(op_->handler()));
            op_ = nullptr;
            op();
        }
    };

    using buf_type = char[sizeof(holder<exemplar>)];

    base* base_ = nullptr;
    alignas(holder<exemplar>) buf_type buf_;

public:
    ~pausation()
    {
        if(base_)
            base_->~base();
    }

    pausation() = default;

    pausation(pausation&& other)
    {
        if(other.base_)
        {
            base_ = other.base_->move(buf_);
            other.base_ = nullptr;
        }
    }

    pausation&
    operator=(pausation&& other)
    {
        // Engaged pausations must be invoked before
        // assignment otherwise the io_service
        // completion invariants are broken.
        BOOST_ASSERT(! base_);

        if(other.base_)
        {
            base_ = other.base_->move(buf_);
            other.base_ = nullptr;
        }
        return *this;
    }

    template<class F>
    void
    emplace(F&& f);

    template<class F>
    void
    save(F&& f);

    bool
    maybe_invoke()
    {
        if(base_)
        {
            auto const basep = base_;
            base_ = nullptr;
            (*basep)();
            return true;
        }
        return false;
    }
};

template<class F>
void
pausation::emplace(F&& f)
{
    using type = holder<typename std::decay<F>::type>;
    static_assert(sizeof(buf_type) >= sizeof(type),
        "buffer too small");
    BOOST_ASSERT(! base_);
    base_ = ::new(buf_) type{std::forward<F>(f)};
}

template<class F>
void
pausation::save(F&& f)
{
    emplace(saved_op<F>{std::move(f)});
}

} // detail
} // websocket
} // beast
} // boost

#endif
