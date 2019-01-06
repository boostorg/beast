//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_IMPL_TEARDOWN_IPP
#define BOOST_BEAST_WEBSOCKET_IMPL_TEARDOWN_IPP

#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/core/detail/async_op_base.hpp>
#include <boost/beast/core/detail/get_executor_type.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/post.hpp>
#include <memory>

namespace boost {
namespace beast {
namespace websocket {

namespace detail {

template<class Handler>
class teardown_tcp_op
    : public beast::detail::async_op_base<
        Handler, beast::detail::get_executor_type<
            net::ip::tcp::socket>>
    , public net::coroutine
{
    using socket_type = net::ip::tcp::socket;

    socket_type& s_;
    role_type role_;
    bool nb_;

public:
    template<class Handler_>
    teardown_tcp_op(
        Handler_&& h,
        socket_type& s,
        role_type role)
        : beast::detail::async_op_base<
            Handler, beast::detail::get_executor_type<
                net::ip::tcp::socket>>(s.get_executor(),
                    std::forward<Handler_>(h))
        , s_(s)
        , role_(role)
    {
    }

    void
    operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0)
    {
        using net::buffer;
        using tcp = net::ip::tcp;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            nb_ = s_.non_blocking();
            s_.non_blocking(true, ec);
            if(! ec)
            {
                if(role_ == role_type::server)
                    s_.shutdown(tcp::socket::shutdown_send, ec);
            }
            if(ec)
            {
                BOOST_ASIO_CORO_YIELD
                net::post(
                    s_.get_executor(),
                    beast::bind_front_handler(std::move(*this), ec, 0));
                goto upcall;
            }
            for(;;)
            {
                {
                    char buf[2048];
                    s_.read_some(
                        net::buffer(buf), ec);
                }
                if(ec == net::error::would_block)
                {
                    BOOST_ASIO_CORO_YIELD
                    s_.async_wait(
                        net::ip::tcp::socket::wait_read,
                            std::move(*this));
                    continue;
                }
                if(ec)
                {
                    if(ec != net::error::eof)
                        goto upcall;
                    ec = {};
                    break;
                }
                if(bytes_transferred == 0)
                {
                    // happens sometimes
                    // https://github.com/boostorg/beast/issues/1373
                    break;
                }
            }
            if(role_ == role_type::client)
                s_.shutdown(tcp::socket::shutdown_send, ec);
            if(ec)
                goto upcall;
            s_.close(ec);
        upcall:
            {
                error_code ignored;
                s_.non_blocking(nb_, ignored);
            }
            this->invoke(ec);
        }
    }
};

} // detail

//------------------------------------------------------------------------------

void
teardown(
    role_type role,
    net::ip::tcp::socket& socket,
    error_code& ec)
{
    using net::buffer;
    if(role == role_type::server)
        socket.shutdown(
            net::ip::tcp::socket::shutdown_send, ec);
    if(ec)
        return;
    for(;;)
    {
        char buf[2048];
        auto const bytes_transferred =
            socket.read_some(buffer(buf), ec);
        if(ec)
        {
            if(ec != net::error::eof)
                return;
            ec = {};
            break;
        }
        if(bytes_transferred == 0)
        {
            // happens sometimes
            // https://github.com/boostorg/beast/issues/1373
            break;
        }
    }
    if(role == role_type::client)
        socket.shutdown(
            net::ip::tcp::socket::shutdown_send, ec);
    if(ec)
        return;
    socket.close(ec);
}

template<class TeardownHandler>
void
async_teardown(
    role_type role,
    net::ip::tcp::socket& socket,
    TeardownHandler&& handler)
{
    static_assert(beast::is_completion_handler<
        TeardownHandler, void(error_code)>::value,
            "TeardownHandler requirements not met");
    detail::teardown_tcp_op<typename std::decay<
        TeardownHandler>::type>(std::forward<
            TeardownHandler>(handler), socket,
                role)();
}

} // websocket
} // beast
} // boost

#endif
