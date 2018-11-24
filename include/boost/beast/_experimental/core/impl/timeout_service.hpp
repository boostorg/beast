//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_IMPL_TIMEOUT_SERVICE_HPP
#define BOOST_BEAST_CORE_IMPL_TIMEOUT_SERVICE_HPP

#include <boost/beast/_experimental/core/detail/timeout_service.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/assert.hpp>

namespace boost {
namespace beast {

timeout_handle::
timeout_handle(boost::asio::io_context& ioc)
    : timeout_handle(
        boost::asio::use_service<
            detail::timeout_service>(
                ioc).make_handle())
{
}

void
timeout_handle::
destroy()
{
    BOOST_ASSERT(svc_ != nullptr);
    svc_->destroy(*this);
    id_ = 0;
    svc_ = nullptr;
}

template<class Executor, class CancelHandler>
void
timeout_handle::
set_callback(
    Executor const& ex, CancelHandler&& handler)
{
    svc_->set_callback(*this, ex,
        std::forward<CancelHandler>(handler));
}

//------------------------------------------------------------------------------

void
set_timeout_service_options(
    boost::asio::io_context& ioc,
    std::chrono::seconds interval)
{
    boost::asio::use_service<
        detail::timeout_service>(
            ioc).set_option(interval);
}

} // beast
} // boost

#endif
