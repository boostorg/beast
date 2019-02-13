//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_IMPL_ACCEPT_IPP
#define BOOST_BEAST_WEBSOCKET_IMPL_ACCEPT_IPP

#include <boost/beast/core/buffer_size.hpp>
#include <boost/beast/websocket/impl/stream_impl.hpp>
#include <boost/beast/websocket/detail/type_traits.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/core/async_op_base.hpp>
#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/core/detail/buffer.hpp>
#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/post.hpp>
#include <boost/assert.hpp>
#include <boost/throw_exception.hpp>
#include <memory>
#include <type_traits>

namespace boost {
namespace beast {
namespace websocket {

/** Respond to an HTTP request
*/
template<class NextLayer, bool deflateSupported>
template<class Handler>
class stream<NextLayer, deflateSupported>::response_op
    : public beast::stable_async_op_base<
        Handler, beast::executor_type<stream>>
    , public net::coroutine
{
    boost::weak_ptr<impl_type> wp_;
    error_code result_; // must come before res_
    response_type& res_;

public:
    template<
        class Handler_,
        class Body, class Allocator,
        class Decorator>
    response_op(
        Handler_&& h,
        boost::shared_ptr<impl_type> const& sp,
        http::request<Body,
            http::basic_fields<Allocator>> const& req,
        Decorator const& decorator,
        bool cont = false)
        : stable_async_op_base<Handler,
            beast::executor_type<stream>>(
                std::forward<Handler_>(h),
                    sp->stream.get_executor())
        , wp_(sp)
        , res_(beast::allocate_stable<response_type>(*this, 
            sp->build_response(req, decorator, result_)))
    {
        (*this)({}, 0, cont);
    }

    void operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0,
        bool cont = true)
    {
        boost::ignore_unused(bytes_transferred);
        auto sp = wp_.lock();
        if(! sp)
            return this->invoke(cont,
                net::error::operation_aborted);
        auto& impl = *sp;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            impl.change_status(status::handshake);
            impl.update_timer(this->get_executor());

            // Send response
            BOOST_ASIO_CORO_YIELD
            http::async_write(
                impl.stream, res_, std::move(*this));
            if(impl.check_stop_now(ec))
                goto upcall;
            if(! ec)
                ec = result_;
            if(! ec)
            {
                impl.do_pmd_config(res_);
                impl.open(role_type::server);
            }
        upcall:
            this->invoke(cont, ec);
        }
    }
};

//------------------------------------------------------------------------------

// read and respond to an upgrade request
//
template<class NextLayer, bool deflateSupported>
template<class Decorator, class Handler>
class stream<NextLayer, deflateSupported>::accept_op
    : public beast::stable_async_op_base<
        Handler, beast::executor_type<stream>>
    , public net::coroutine
{
    boost::weak_ptr<impl_type> wp_;
    http::request_parser<http::empty_body>& p_;
    Decorator d_;

public:
    template<class Handler_, class Buffers>
    accept_op(
        Handler_&& h,
        boost::shared_ptr<impl_type> const& sp,
        Buffers const& buffers,
        Decorator const& decorator)
        : stable_async_op_base<Handler,
            beast::executor_type<stream>>(
                std::forward<Handler_>(h),
                    sp->stream.get_executor())
        , wp_(sp)
        , p_(beast::allocate_stable<
            http::request_parser<http::empty_body>>(*this))
        , d_(decorator)
    {
        auto& impl = *sp;
        error_code ec;
        auto const mb =
            beast::detail::dynamic_buffer_prepare(
            impl.rd_buf, buffer_size(buffers),
                ec, error::buffer_overflow);
        if(! ec)
            impl.rd_buf.commit(
                net::buffer_copy(*mb, buffers));
        (*this)(ec);
    }

    void operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0,
        bool cont = true)
    {
        boost::ignore_unused(bytes_transferred);
        auto sp = wp_.lock();
        if(! sp)
            return this->invoke(cont,
                net::error::operation_aborted);
        auto& impl = *sp;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            impl.change_status(status::handshake);
            impl.update_timer(this->get_executor());

            // The constructor could have set ec
            if(ec)
                goto upcall;

            BOOST_ASIO_CORO_YIELD
            http::async_read(impl.stream,
                impl.rd_buf, p_, std::move(*this));
            if(ec == http::error::end_of_stream)
                ec = error::closed;
            if(impl.check_stop_now(ec))
                goto upcall;
            
            {
                // Arguments from our state must be
                // moved to the stack before releasing
                // the handler.
                auto const req = p_.release();
                auto const decorator = d_;
                response_op<Handler>(
                    this->release_handler(),
                        sp, req, decorator, true);
                return;
            }

        upcall:
            this->invoke(cont, ec);
        }
    }
};

//------------------------------------------------------------------------------

template<class NextLayer, bool deflateSupported>
template<class Body, class Allocator,
    class Decorator>
void
stream<NextLayer, deflateSupported>::
do_accept(
    http::request<Body,
        http::basic_fields<Allocator>> const& req,
    Decorator const& decorator,
    error_code& ec)
{
    impl_->change_status(status::handshake);

    error_code result;
    auto const res = impl_->build_response(req, decorator, result);
    http::write(impl_->stream, res, ec);
    if(ec)
        return;
    ec = result;
    if(ec)
    {
        // VFALCO TODO Respect keep alive setting, perform
        //             teardown if Connection: close.
        return;
    }
    impl_->do_pmd_config(res);
    impl_->open(role_type::server);
}

template<class NextLayer, bool deflateSupported>
template<class Buffers, class Decorator>
void
stream<NextLayer, deflateSupported>::
do_accept(
    Buffers const& buffers,
    Decorator const& decorator,
    error_code& ec)
{
    impl_->reset();
    auto const mb =
        beast::detail::dynamic_buffer_prepare(
        impl_->rd_buf, buffer_size(buffers), ec,
            error::buffer_overflow);
    if(ec)
        return;
    impl_->rd_buf.commit(net::buffer_copy(*mb, buffers));

    http::request_parser<http::empty_body> p;
    http::read(next_layer(), impl_->rd_buf, p, ec);
    if(ec == http::error::end_of_stream)
        ec = error::closed;
    if(ec)
        return;
    do_accept(p.get(), decorator, ec);
}

//------------------------------------------------------------------------------

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
accept()
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    error_code ec;
    accept(ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer, bool deflateSupported>
template<class ResponseDecorator>
void
stream<NextLayer, deflateSupported>::
accept_ex(ResponseDecorator const& decorator)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(detail::is_response_decorator<
        ResponseDecorator>::value,
            "ResponseDecorator requirements not met");
    error_code ec;
    accept_ex(decorator, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
accept(error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    do_accept(
        net::const_buffer{},
        &default_decorate_res, ec);
}

template<class NextLayer, bool deflateSupported>
template<class ResponseDecorator>
void
stream<NextLayer, deflateSupported>::
accept_ex(ResponseDecorator const& decorator, error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(detail::is_response_decorator<
        ResponseDecorator>::value,
            "ResponseDecorator requirements not met");
    do_accept(
        net::const_buffer{},
        decorator, ec);
}

template<class NextLayer, bool deflateSupported>
template<class ConstBufferSequence>
typename std::enable_if<! http::detail::is_header<
    ConstBufferSequence>::value>::type
stream<NextLayer, deflateSupported>::
accept(ConstBufferSequence const& buffers)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(net::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    error_code ec;
    accept(buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer, bool deflateSupported>
template<
    class ConstBufferSequence,
    class ResponseDecorator>
typename std::enable_if<! http::detail::is_header<
    ConstBufferSequence>::value>::type
stream<NextLayer, deflateSupported>::
accept_ex(
    ConstBufferSequence const& buffers,
    ResponseDecorator const &decorator)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(net::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    static_assert(detail::is_response_decorator<
        ResponseDecorator>::value,
            "ResponseDecorator requirements not met");
    error_code ec;
    accept_ex(buffers, decorator, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer, bool deflateSupported>
template<class ConstBufferSequence>
typename std::enable_if<! http::detail::is_header<
    ConstBufferSequence>::value>::type
stream<NextLayer, deflateSupported>::
accept(
    ConstBufferSequence const& buffers, error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(net::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    do_accept(buffers, &default_decorate_res, ec);
}

template<class NextLayer, bool deflateSupported>
template<
    class ConstBufferSequence,
    class ResponseDecorator>
typename std::enable_if<! http::detail::is_header<
    ConstBufferSequence>::value>::type
stream<NextLayer, deflateSupported>::
accept_ex(
    ConstBufferSequence const& buffers,
    ResponseDecorator const& decorator,
    error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(net::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    static_assert(net::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    do_accept(buffers, decorator, ec);
}

template<class NextLayer, bool deflateSupported>
template<class Body, class Allocator>
void
stream<NextLayer, deflateSupported>::
accept(
    http::request<Body,
        http::basic_fields<Allocator>> const& req)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    error_code ec;
    accept(req, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer, bool deflateSupported>
template<
    class Body, class Allocator,
    class ResponseDecorator>
void
stream<NextLayer, deflateSupported>::
accept_ex(
    http::request<Body,
        http::basic_fields<Allocator>> const& req,
    ResponseDecorator const& decorator)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(detail::is_response_decorator<
        ResponseDecorator>::value,
            "ResponseDecorator requirements not met");
    error_code ec;
    accept_ex(req, decorator, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer, bool deflateSupported>
template<class Body, class Allocator>
void
stream<NextLayer, deflateSupported>::
accept(
    http::request<Body,
        http::basic_fields<Allocator>> const& req,
    error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    impl_->reset();
    do_accept(req, &default_decorate_res, ec);
}

template<class NextLayer, bool deflateSupported>
template<
    class Body, class Allocator,
    class ResponseDecorator>
void
stream<NextLayer, deflateSupported>::
accept_ex(
    http::request<Body,
        http::basic_fields<Allocator>> const& req,
    ResponseDecorator const& decorator,
    error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(detail::is_response_decorator<
        ResponseDecorator>::value,
            "ResponseDecorator requirements not met");
    impl_->reset();
    do_accept(req, decorator, ec);
}

//------------------------------------------------------------------------------

template<class NextLayer, bool deflateSupported>
template<
    class AcceptHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    AcceptHandler, void(error_code))
stream<NextLayer, deflateSupported>::
async_accept(
    AcceptHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        AcceptHandler, void(error_code));
    impl_->reset();
    accept_op<
        decltype(&default_decorate_res),
        BOOST_ASIO_HANDLER_TYPE(
            AcceptHandler, void(error_code))>(
                std::move(init.completion_handler),
                impl_, net::const_buffer{},
                &default_decorate_res);;
    return init.result.get();
}

template<class NextLayer, bool deflateSupported>
template<
    class ResponseDecorator,
    class AcceptHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    AcceptHandler, void(error_code))
stream<NextLayer, deflateSupported>::
async_accept_ex(
    ResponseDecorator const& decorator,
    AcceptHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    static_assert(detail::is_response_decorator<
        ResponseDecorator>::value,
            "ResponseDecorator requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        AcceptHandler, void(error_code));
    impl_->reset();
    accept_op<
        ResponseDecorator,
        BOOST_ASIO_HANDLER_TYPE(
            AcceptHandler, void(error_code))>(
                std::move(init.completion_handler),
                impl_, net::const_buffer{},
                decorator);
    return init.result.get();
}

template<class NextLayer, bool deflateSupported>
template<
    class ConstBufferSequence,
    class AcceptHandler>
typename std::enable_if<
    ! http::detail::is_header<ConstBufferSequence>::value,
    BOOST_ASIO_INITFN_RESULT_TYPE(
        AcceptHandler, void(error_code))>::type
stream<NextLayer, deflateSupported>::
async_accept(
    ConstBufferSequence const& buffers,
    AcceptHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    static_assert(net::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        AcceptHandler, void(error_code));
    impl_->reset();
    accept_op<
        decltype(&default_decorate_res),
        BOOST_ASIO_HANDLER_TYPE(
            AcceptHandler, void(error_code))>(
                std::move(init.completion_handler),
                    impl_, buffers, &default_decorate_res);
    return init.result.get();
}

template<class NextLayer, bool deflateSupported>
template<
    class ConstBufferSequence,
    class ResponseDecorator,
    class AcceptHandler>
typename std::enable_if<
    ! http::detail::is_header<ConstBufferSequence>::value,
    BOOST_ASIO_INITFN_RESULT_TYPE(
        AcceptHandler, void(error_code))>::type
stream<NextLayer, deflateSupported>::
async_accept_ex(
    ConstBufferSequence const& buffers,
    ResponseDecorator const& decorator,
    AcceptHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    static_assert(net::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    static_assert(detail::is_response_decorator<
        ResponseDecorator>::value,
            "ResponseDecorator requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        AcceptHandler, void(error_code));
    impl_->reset();
    accept_op<
        ResponseDecorator,
        BOOST_ASIO_HANDLER_TYPE(
            AcceptHandler, void(error_code))>(
                std::move(init.completion_handler),
                    impl_, buffers, decorator);
    return init.result.get();
}

template<class NextLayer, bool deflateSupported>
template<
    class Body, class Allocator,
    class AcceptHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    AcceptHandler, void(error_code))
stream<NextLayer, deflateSupported>::
async_accept(
    http::request<Body, http::basic_fields<Allocator>> const& req,
    AcceptHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        AcceptHandler, void(error_code));
    impl_->reset();
    response_op<
        BOOST_ASIO_HANDLER_TYPE(
            AcceptHandler, void(error_code))>(
                std::move(init.completion_handler),
                    impl_, req, &default_decorate_res);
    return init.result.get();
}

template<class NextLayer, bool deflateSupported>
template<
    class Body, class Allocator,
    class ResponseDecorator,
    class AcceptHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    AcceptHandler, void(error_code))
stream<NextLayer, deflateSupported>::
async_accept_ex(
    http::request<Body, http::basic_fields<Allocator>> const& req,
    ResponseDecorator const& decorator,
    AcceptHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    static_assert(detail::is_response_decorator<
        ResponseDecorator>::value,
            "ResponseDecorator requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        AcceptHandler, void(error_code));
    impl_->reset();
    response_op<
        BOOST_ASIO_HANDLER_TYPE(
            AcceptHandler, void(error_code))>(
                std::move(init.completion_handler),
                    impl_, req, decorator);
    return init.result.get();
}

} // websocket
} // beast
} // boost

#endif
