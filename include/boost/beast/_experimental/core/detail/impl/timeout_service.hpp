//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DETAIL_IMPL_TIMEOUT_SERVICE_HPP
#define BOOST_BEAST_CORE_DETAIL_IMPL_TIMEOUT_SERVICE_HPP

namespace boost {
namespace beast {
namespace detail {

timeout_service::
timeout_service(net::io_context& ctx)
    : service_base(ctx)
    , thunks_(1) // element [0] reserved for "null"
    , timer_(ctx)
{
}

timeout_handle
timeout_service::
make_handle()
{
    std::lock_guard<std::mutex> lock(m_);
    if(free_thunk_ != 0)
    {
        auto const n = free_thunk_;
        auto& t = thunks_[n];
        free_thunk_ = t.pos; // next in free list
        t = {};
        return timeout_handle(n, *this);
    }
    auto const n = thunks_.size();
    thunks_.emplace_back();
    return timeout_handle(n, *this);
}

void
timeout_service::
set_option(std::chrono::seconds n)
{
    interval_ = n;
}

template<class Executor, class CancelHandler>
void
timeout_service::
set_callback(
    timeout_handle h,
    Executor const& ex,
    CancelHandler&& handler)
{
    thunks_[h.id_].callback.emplace(
        callback<
            Executor,
            typename std::decay<CancelHandler>::type>{
        h, ex,
        std::forward<CancelHandler>(handler)});
}

void
timeout_service::
on_work_started(timeout_handle h)
{
    BOOST_ASSERT(h.id_ != 0);
    if( [this, h]
        {
            std::lock_guard<std::mutex> lock(m_);
            auto& t = thunks_[h.id_];
            insert(t, *fresh_);
            return ++pending_ == 1;
        }())
    {
        do_async_wait();
    }
}

void
timeout_service::
on_work_stopped(timeout_handle h)
{
    BOOST_ASSERT(h.id_ != 0);
    std::lock_guard<std::mutex> lock(m_);
    auto& t = thunks_[h.id_];
    if(t.list != nullptr)
    {
        BOOST_ASSERT(! t.expired);
        remove(t);
    }
    if(--pending_ == 0)
        timer_.cancel();
    BOOST_ASSERT(pending_ >= 0);
}

/*
    Synchronization points

    (A) async_op invoke
    (B) timeout handler invoke  expired=true
    (C) posted handler invoked  canceled=true

    ----------------------------------------------

    Linearized paths (for async_read)

    ----------------------------------------------

    1. async_read
    2. async_read complete, async_op posted
(A) 3. async_op invoked
        - work_.try_complete() returns true
            + expired==false
            + thunk is removed from list

    ----------------------------------------------

    1. async_read
    2. async_read complete, async_op posted
(B) 3. timeout, cancel posted
        - expired=true
        - thunk is removed from list
(A) 4. async_op invoked
        - work_.try_complete() returns false
            + completed=true
        - handler is saved
(C) 5. cancel invoked
        - saved handler is invoked
            + expired==true, canceled==false, completed==true
            + work_.try_complete() returns true

    ----------------------------------------------

    The following two paths are not distinguishable:

    1. async_read
(B) 2. timeout, cancel posted
        - expired=true
        - thunk is removed from list
    3. async_read complete, async_op posted
(C) 4. cancel invoked
        - socket::cancel called (what does this do?)
        - canceled=true
(A) 5. async_op invoked
        - expired==true, canceled==true, completed==false
        - work_.try_complete() returns `true`

    1. async_read
(B) 2. timeout, `cancel` posted
        - expired=true
        - thunk is removed from list
(C) 3. cancel invoked, async_read canceled
        - socket::cancel called
        - canceled=true
(A) 4. async_op invoked, ec==operation_aborted
        - expired==true, canceled==true, completed=false
        - work_.try_complete()` returns true
*/
bool
timeout_service::
on_try_work_complete(timeout_handle h)
{
    BOOST_ASSERT(h.id_ != 0);
    std::lock_guard<std::mutex> lock(m_);
    auto& t = thunks_[h.id_];
    if(! t.expired)
    {
        // hot path: operation complete
        BOOST_ASSERT(t.list != nullptr);
        BOOST_ASSERT(! t.canceled);
        BOOST_ASSERT(! t.completed);
        remove(t);
        return true;
    }
    BOOST_ASSERT(t.list == nullptr);
    if(! t.canceled)
    {
        // happens when operation completes before
        // posted cancel handler is invoked.
        t.completed = true;
        return false;
    }
    if(t.completed)
    {
        // happens when the saved handler is
        // invoked from the posted callback
        t.expired = false;
        t.canceled = false;
        t.completed = false;
        return true;
    }
    // happens when operation_aborted is delivered
    t.expired = false;
    t.canceled = false;
    return true;
}

void
timeout_service::
on_cancel(timeout_handle h)
{
    std::lock_guard<std::mutex> lock(m_);
    auto& t = thunks_[h.id_];
    BOOST_ASSERT(t.expired);
    t.canceled = true;
}

//------------------------------------------------------------------------------

void
timeout_service::
destroy(timeout_handle h)
{
    BOOST_ASSERT(h.id_ != 0);
    std::lock_guard<std::mutex> lock(m_);
    thunks_[h.id_].pos = free_thunk_;
    free_thunk_ = h.id_;
}

// Precondition: caller holds the mutex
void
timeout_service::
insert(
    thunk& t,
    thunk::list_type& list)
{
    BOOST_ASSERT(t.list == nullptr);
    list.emplace_back(&t); // can throw
    t.list = &list;
    t.pos = list.size();
}

// Precondition: caller holds the mutex
void
timeout_service::
remove(thunk& t)
{
    BOOST_ASSERT(t.list != nullptr);
    BOOST_ASSERT(
        t.list == stale_ ||
        t.list == fresh_);
    BOOST_ASSERT(t.list->size() > 0);
    auto& list = *t.list;
    auto const n = list.size() - 1;
    if(t.pos != n)
    {
        // move back element to t.pos
        list[t.pos] = list[n];
        list[t.pos]->pos = t.pos;
    }
    t.list = nullptr;
    list.resize(n);
}

void
timeout_service::
do_async_wait()
{
    timer_.expires_after(interval_);
    timer_.async_wait(
        [this](error_code ec)
        {
            this->on_timer(ec);
        });
}

void
timeout_service::
on_timer(error_code ec)
{
    if(ec == net::error::operation_aborted)
    {
        BOOST_ASSERT(fresh_->empty());
        BOOST_ASSERT(stale_->empty());
        return;
    }
    std::vector<thunk*> expired;
    {
        std::lock_guard<std::mutex> lock(m_);
        if(! stale_->empty())
        {
            for(auto t : *stale_)
            {
                // remove from list
                t->list = nullptr;
                t->expired = true;
            }
            std::swap(expired, *stale_);
            stale_->reserve(expired.size() / 2);
        }
        std::swap(fresh_, stale_);
    }
    for(auto p : expired)
        p->callback();
    if( [this]
        {
            std::lock_guard<std::mutex> lock(m_);
            BOOST_ASSERT(pending_);
            pending_ =
                ! stale_->empty() ||
                ! fresh_->empty();
            return pending_;
        }())
    {
        do_async_wait();
    }
}

void
timeout_service::
shutdown() noexcept
{
    // The ExecutionContext is already in a stopped
    // state, so no synchronization is required.
    timer_.cancel();
}

} // detail
} // beast
} // boost

#endif
