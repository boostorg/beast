//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_TIMEOUT_SERVICE_HPP
#define BOOST_BEAST_CORE_TIMEOUT_SERVICE_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/io_context.hpp> // #include <boost/asio/execution_context.hpp>
#include <cstddef>

namespace boost {
namespace beast {

namespace detail {
class timeout_service;
} // detail

class timeout_work_guard;

class timeout_handle
{
    std::size_t id_ = 0;
    detail::timeout_service* svc_ = nullptr;

    timeout_handle(
        std::size_t id,
        detail::timeout_service& svc)
        : id_(id)
        , svc_(&svc)
    {
    }

    detail::timeout_service&
    service() const
    {
        return *svc_;
    }

    friend class detail::timeout_service;
    friend class timeout_work_guard;

public:
    timeout_handle() = default;
    timeout_handle(timeout_handle const&) = default;
    timeout_handle& operator=(timeout_handle const&) = default;

    timeout_handle(std::nullptr_t)
    {
    }

    timeout_handle&
    operator=(std::nullptr_t)
    {
        id_ = 0;
        svc_ = nullptr;
        return *this;
    }

    // VFALCO should be execution_context
    BOOST_BEAST_DECL
    explicit
    timeout_handle(net::io_context& ioc);

    BOOST_BEAST_DECL
    void
    destroy();

    template<class Executor, class CancelHandler>
    void
    set_callback(
        Executor const& ex, CancelHandler&& handler);

    explicit
    operator bool() const noexcept
    {
        return svc_ != nullptr;
    }

    friend bool operator==(
        timeout_handle const& lhs,
        std::nullptr_t) noexcept
    {
        return lhs.svc_ == nullptr;
    }

    friend bool operator==(
        std::nullptr_t,
        timeout_handle const& rhs) noexcept
    {
        return rhs.svc_ == nullptr;
    }

    friend bool operator!=(
        timeout_handle const& lhs,
        std::nullptr_t) noexcept
    {
        return lhs.svc_ != nullptr;
    }

    friend bool operator!=(
        std::nullptr_t,
        timeout_handle const& rhs) noexcept
    {
        return rhs.svc_ != nullptr;
    }
};

/** Set timeout service options in an execution context.

    This changes the time interval for all timeouts associated
    with the execution context. The option must be set before any
    timeout objects are constructed.

    @param ctx The execution context.

    @param interval The approximate amount of time until a timeout occurs.
*/
BOOST_BEAST_DECL
void
set_timeout_service_options(
    net::io_context& ctx, // VFALCO should be execution_context
    std::chrono::seconds interval);

} // beast
} // boost

#include <boost/beast/_experimental/core/impl/timeout_service.hpp>

#endif
