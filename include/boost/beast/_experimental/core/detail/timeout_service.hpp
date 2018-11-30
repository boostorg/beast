//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DETAIL_TIMEOUT_SERVICE_HPP
#define BOOST_BEAST_CORE_DETAIL_TIMEOUT_SERVICE_HPP

#include <boost/beast/_experimental/core/detail/service_base.hpp>
#include <boost/beast/_experimental/core/detail/timeout_service_base.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/io_context.hpp> // #include <boost/asio/execution_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <utility>
#include <vector>

namespace boost {
namespace beast {

class timeout_handle;

namespace detail {

class timeout_service
    : public service_base<timeout_service>
{
    template<class Executor, class Handler>
    struct callback
    {
        timeout_handle th;
        Executor ex;
        Handler h;

        void
        operator()()
        {
            net::post(ex, 
                beast::bind_front_handler(
                    std::move(*this), 0));
        }

        void
        operator()(int)
        {
            th.service().on_cancel(th);
            h();
        }
    };

public:
    using key_type = timeout_service;

    // VFALCO Should be execution_context
    BOOST_BEAST_DECL
    explicit
    timeout_service(net::io_context& ctx);

    BOOST_BEAST_DECL
    timeout_handle
    make_handle();

    BOOST_BEAST_DECL
    void set_option(std::chrono::seconds n);

    // Undefined if work is active
    template<class Executor, class CancelHandler>
    void set_callback(
        timeout_handle h,
        Executor const& ex,
        CancelHandler&& handler);

    BOOST_BEAST_DECL
    void on_work_started(timeout_handle h);

    BOOST_BEAST_DECL
    void on_work_stopped(timeout_handle h);

    BOOST_BEAST_DECL
    bool on_try_work_complete(timeout_handle h);

private:
    friend class beast::timeout_handle;

    BOOST_BEAST_DECL
    void destroy(timeout_handle h);

    BOOST_BEAST_DECL
    void insert(thunk& t, thunk::list_type& list);

    BOOST_BEAST_DECL
    void remove(thunk& t);

    BOOST_BEAST_DECL
    void do_async_wait();

    BOOST_BEAST_DECL
    void on_cancel(timeout_handle h);

    BOOST_BEAST_DECL
    void on_timer(error_code ec);

    BOOST_BEAST_DECL
    virtual void shutdown() noexcept override;

    std::mutex m_;
    thunk::list_type list_[2];
    thunk::list_type* fresh_ = &list_[0];
    thunk::list_type* stale_ = &list_[1];
    std::deque<thunk> thunks_;
    std::size_t free_thunk_ = 0;
    net::steady_timer timer_;
    std::chrono::seconds interval_{30ul};
    long pending_ = 0;
};

} // detail
} // beast
} // boost

#include <boost/beast/_experimental/core/detail/impl/timeout_service.hpp>

#endif
