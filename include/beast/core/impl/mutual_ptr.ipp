//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_IMPL_MUTUAL_PTR_HPP
#define BEAST_IMPL_MUTUAL_PTR_HPP

#include <beast/core/detail/empty_base_optimization.hpp>
#include <boost/assert.hpp>
#include <memory>

namespace beast {

template<class T>
template<class Alloc>
struct mutual_ptr<T>::D :
    detail::empty_base_optimization<Alloc>, B
{
    using A = typename std::allocator_traits<Alloc>::
        template rebind_alloc<D>;

    template<class... Args>
    explicit
    D(Alloc const& a, Args&&... args)
        : empty_base_optimization<Alloc>(a)
        , B(std::forward<Args>(args)...)
    {
    }

    void
    destroy() override
    {
        auto a = A{this->member()};
        this->~D();
        std::allocator_traits<A>::deallocate(
            a, this, 1);
    }
};

template<class T>
inline
void
mutual_ptr<T>::
release()
{
    BOOST_ASSERT(p_);
    if(--p_->n > 0)
    {
        if(prev_)
            prev_->next_ = next_;
        if(next_)
            next_->prev_ = prev_;
        return;
    }
    p_->destroy();
}

template<class T>
mutual_ptr<T>::
~mutual_ptr()
{
    if(p_)
        release();
}

template<class T>
mutual_ptr<T>::
mutual_ptr()
    : p_(nullptr)
{
}

template<class T>
mutual_ptr<T>::
mutual_ptr(mutual_ptr&& other)
    : p_(other.p_)
{
    if(! p_)
        return;
    other.p_ = nullptr;
    prev_ = other.prev_;
    next_ = other.next_;
    if(prev_)
        prev_->next_ = this;
    if(next_)
        next_->prev_ = this;
}

template<class T>
mutual_ptr<T>::
mutual_ptr(mutual_ptr const& other)
    : p_(other.p_)
{
    if(! p_)
        return;
    ++p_->n;
    prev_ = other.prev_;
    if(prev_)
        prev_->next_ = this;
    next_ =
        const_cast<mutual_ptr*>(&other);
    next_->prev_ = this;
}

template<class T>
mutual_ptr<T>&
mutual_ptr<T>::
operator=(mutual_ptr&& other)
{
    if(p_)
        release();
    p_ = other.p_;
    other.p_ = nullptr;
    prev_ = other.prev_;
    next_ = other.next_;
    if(prev_)
        prev_->next_ = this;
    if(next_)
        next_->prev_ = this;
    return *this;
}

template<class T>
mutual_ptr<T>&
mutual_ptr<T>::
operator=(mutual_ptr const& other)
{
    if(this != &other)
    {
        if(p_)
            release();
        p_ = other.p_;
        ++p_->n;
        prev_ = other.prev_;
        if(prev_)
            prev_->next_ = this;
        next_ =
            const_cast<mutual_ptr*>(&other);
        next_->prev_ = this;
    }
    return *this;
}

template<class T>
void
mutual_ptr<T>::
reset()
{
    if(p_)
    {
        release();
        p_ = nullptr;
    }
}

template<class T>
void
mutual_ptr<T>::
reset_all()
{
    BOOST_ASSERT(p_);
    auto it = prev_;
    while(it)
    {
        auto cur = it;
        it = it->prev_;
        BOOST_ASSERT(cur->p_);
        cur->release();
        cur->p_ = nullptr;
    }
    it = this;
    while(it)
    {
        auto cur = it;
        it = it->next_;
        BOOST_ASSERT(cur->p_);
        cur->release();
        cur->p_ = nullptr;
    }
}

template<class T>
template<class Alloc, class... Args>
mutual_ptr<T>::
mutual_ptr(detail::mutual_ptr_alloc,
    Alloc const& alloc, Args&&... args)
{
    using A = typename D<Alloc>::A;
    auto a = A{alloc};
    auto p = std::allocator_traits<
        A>::allocate(a, 1);
    try
    {
        new(p) D<Alloc>(alloc,
            std::forward<Args>(args)...);
    }
    catch(...)
    {
        std::allocator_traits<
            A>::deallocate(a, p, 1);
        throw;
    }
    p_ = p;
    next_ = nullptr;
    prev_ = nullptr;
}

} // beast

#endif
