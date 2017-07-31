//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_EXAMPLE_COMMON_SESSION_ALLOC_HPP
#define BOOST_BEAST_EXAMPLE_COMMON_SESSION_ALLOC_HPP

#include <boost/asio/handler_alloc_hook.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/assert.hpp>
#include <boost/intrusive/list.hpp>
#include <algorithm>
#include <cstddef>
#include <utility>

template<class Context>
class session_alloc_base
{
    template<class Handler>
    class wrapped_handler;

    class pool_t
    {
        using hook_type = 
            boost::intrusive::list_base_hook<
                boost::intrusive::link_mode<
                    boost::intrusive::normal_link>>;

        class element : public hook_type
        {
            std::size_t size_;
            std::size_t used_;
            // add padding here
        
        public:
            explicit
            element(std::size_t size, std::size_t used)
                : size_(size)
                , used_(used)
            {
            }

            std::size_t
            size() const
            {
                return size_;
            }

            std::size_t
            used() const
            {
                return used_;
            }

            char*
            end() const
            {
                return data() + size_;
            }

            char*
            data() const
            {
                return const_cast<char*>(
                    reinterpret_cast<
                        char const *>(this + 1));
            }
        };

        using list_type = typename
            boost::intrusive::make_list<element,
                boost::intrusive::constant_time_size<
                    true>>::type;

        Context* ctx_;
        std::size_t refs_ = 1;      // shared count
        std::size_t high_ = 0;      // highest used
        std::size_t size_ = 0;      // size of buf_
        char* buf_ = nullptr;       // a large block
        list_type list_;            // list of allocations

        explicit
        pool_t(Context* ctx)
            : ctx_(ctx)
        {
        }

    public:
        static
        pool_t&
        construct(Context* ctx);

        ~pool_t();

        pool_t&
        addref();

        void
        release();

        void*
        alloc(std::size_t n);

        void
        dealloc(void* pv, std::size_t n);
    };

    pool_t& pool_;

public:
    session_alloc_base& operator=(session_alloc_base const&) = delete;

    ~session_alloc_base()
    {
        pool_.release();
    }

    session_alloc_base()
        : pool_(pool_t::construct(nullptr))
    {
        static_assert(std::is_same<Context, std::nullptr_t>::value,
            "Context requirements not met");
    }

    session_alloc_base(session_alloc_base const& other)
        : pool_(other.pool_.addref())
    {
    }

    template<class DeducedContext, class = typename
        std::enable_if<! std::is_same<
            session_alloc_base<Context>,
            typename std::decay<DeducedContext>::type
                >::value>::type>
    explicit
    session_alloc_base(DeducedContext& ctx)
        : pool_(pool_t::construct(std::addressof(ctx)))
    {
        static_assert(! std::is_same<Context, std::nullptr_t>::value,
            "Context requirements not met");
    }

    template<class Handler>
    wrapped_handler<typename std::decay<Handler>::type>
    wrap(Handler&& handler)
    {
        return wrapped_handler<
            typename std::decay<Handler>::type>(
                std::forward<Handler>(handler), *this);
    }

protected:
    void*
    alloc(std::size_t n)
    {
        return pool_.alloc(n);
    }

    void
    dealloc(void* p, std::size_t n)
    {
        pool_.dealloc(p, n);
    }
};

//------------------------------------------------------------------------------

template<class T, class Context = std::nullptr_t>
class session_alloc : public session_alloc_base<Context>
{
    template<class U, class C>
    friend class session_alloc;

public:
    using value_type = T;
    using is_always_equal = std::false_type;
    using pointer = T*;
    using reference = T&;
    using const_pointer = T const*;
    using const_reference = T const&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template<class U>
    struct rebind
    {
        using other = session_alloc<U>;
    };

    session_alloc() = default;
    session_alloc(session_alloc const&) = default;

    template<class U>
    session_alloc(session_alloc<U> const& other)
        : session_alloc_base<Context>(static_cast<
            session_alloc_base<Context> const&>(other))
    {
    }

    explicit
    session_alloc(Context& ctx)
        : session_alloc_base<Context>(ctx)
    {
    }

    value_type*
    allocate(size_type n)
    {
        return static_cast<value_type*>(
            this->alloc(n * sizeof(T)));
    }

    void
    deallocate(value_type* p, size_type n)
    {
        this->dealloc(p, n * sizeof(T));
    }

#if defined(BOOST_LIBSTDCXX_VERSION) && BOOST_LIBSTDCXX_VERSION < 60000
    template<class U, class... Args>
    void
    construct(U* ptr, Args&&... args)
    {
        ::new((void*)ptr) U(std::forward<Args>(args)...);
    }

    template<class U>
    void
    destroy(U* ptr)
    {
        ptr->~U();
    }
#endif

    template<class U>
    friend
    bool
    operator==(
        session_alloc const& lhs,
        session_alloc<U> const& rhs)
    {
        return &lhs.pool_ == &rhs.pool_;
    }

    template<class U>
    friend
    bool
    operator!=(
        session_alloc const& lhs,
        session_alloc<U> const& rhs)
    {
        return ! (lhs == rhs);
    }
};

//------------------------------------------------------------------------------

template<class Context>
template<class Handler>
class session_alloc_base<Context>::wrapped_handler
{
    Handler h_;
    session_alloc_base alloc_;

    void*
    alloc(std::size_t size)
    {
        return alloc_.alloc(size);
    }

    void
    dealloc(void* p, std::size_t size)
    {
        alloc_.dealloc(p, size);
    }

public:
    wrapped_handler(wrapped_handler&&) = default;
    wrapped_handler(wrapped_handler const&) = default;

    template<class DeducedHandler>
    explicit
    wrapped_handler(DeducedHandler&& h,
            session_alloc_base const& alloc)
        : h_(std::forward<DeducedHandler>(h))
        , alloc_(alloc)
    {
    }

    template<class... Args>
    void
    operator()(Args&&... args) const
    {
        h_(std::forward<Args>(args)...);
    }

    friend
    void*
    asio_handler_allocate(
        std::size_t size, wrapped_handler* w)
    {
        return w->alloc(size);
    }

    friend
    void
    asio_handler_deallocate(
        void* p, std::size_t size, wrapped_handler* w)
    {
        w->dealloc(p, size);
    }

    friend
    bool
    asio_handler_is_continuation(wrapped_handler* w)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(std::addressof(w->h_));
    }

    template<class F>
    friend
    void
    asio_handler_invoke(F&& f, wrapped_handler* w)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(
            f, std::addressof(w->h_));
    }
};

//------------------------------------------------------------------------------

template<class Context>
auto
session_alloc_base<Context>::
pool_t::
construct(Context* ctx) ->
    pool_t&
{
    using boost::asio::asio_handler_allocate;
    return *new(asio_handler_allocate(
        sizeof(pool_t), ctx)) pool_t{ctx};
}

template<class Context>
session_alloc_base<Context>::
pool_t::
~pool_t()
{
    BOOST_ASSERT(list_.size() == 0);
    if(buf_)
    {
        using boost::asio::asio_handler_deallocate;
        asio_handler_deallocate(buf_, size_, ctx_);
    }
}

template<class Context>
auto
session_alloc_base<Context>::
pool_t::
addref() ->
    pool_t&
{
    ++refs_;
    return *this;
}

template<class Context>
void
session_alloc_base<Context>::
pool_t::
release()
{
    if(--refs_)
        return;
    this->~pool_t();
    using boost::asio::asio_handler_deallocate;
    asio_handler_deallocate(this, sizeof(*this), ctx_);
}

template<class Context>
void*
session_alloc_base<Context>::
pool_t::
alloc(std::size_t n)
{
    if(list_.empty() && size_ < high_)
    {
        if(buf_)
        {
            using boost::asio::asio_handler_deallocate;
            asio_handler_deallocate(buf_, size_, ctx_);
        }
        using boost::asio::asio_handler_allocate;
        buf_ = reinterpret_cast<char*>(
            asio_handler_allocate(high_, ctx_));
        size_ = high_;
    }
    if(buf_)
    {
        char* end;
        std::size_t used;
        if(list_.empty())
        {
            end = buf_;
            used = sizeof(element) + n;
        }
        else
        {
            end = list_.back().end();
            used = list_.back().used() +
                sizeof(element) + n;
        }
        if(end >= buf_ && end +
            sizeof(element) + n <= buf_ + size_)
        {
            auto& e = *new(end) element{n, used};
            list_.push_back(e);
            high_ = (std::max)(high_, used);
            return e.data();
        }
    }
    std::size_t const used =
        sizeof(element) + n + (
            buf_ && ! list_.empty() ?
                list_.back().used() : 0);
    using boost::asio::asio_handler_allocate;
    auto& e = *new(asio_handler_allocate(
        sizeof(element) + n, ctx_)) element{n, used};
    list_.push_back(e);
    high_ = (std::max)(high_, used);
    return e.data();
}

template<class Context>
void
session_alloc_base<Context>::
pool_t::
dealloc(void* pv, std::size_t n)
{
    auto& e = *(reinterpret_cast<element*>(pv) - 1);
    BOOST_ASSERT(e.size() == n);
    if( (e.end() > buf_ + size_) ||
        reinterpret_cast<char*>(&e) < buf_)
    {
        list_.erase(list_.iterator_to(e));
        e.~element();
        using boost::asio::asio_handler_deallocate;
        asio_handler_deallocate(
            &e, sizeof(e) + n, ctx_);
        return;
    }
    list_.erase(list_.iterator_to(e));
    e.~element();
}

#endif
