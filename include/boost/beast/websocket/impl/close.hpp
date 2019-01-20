//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_IMPL_CLOSE_HPP
#define BOOST_BEAST_WEBSOCKET_IMPL_CLOSE_HPP

#include <boost/beast/websocket/teardown.hpp>
#include <boost/beast/websocket/detail/mask.hpp>
#include <boost/beast/core/async_op_base.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/core/detail/bind_continuation.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/post.hpp>
#include <boost/throw_exception.hpp>
#include <memory>

namespace boost {
namespace beast {
namespace websocket {

/*  Close the WebSocket Connection

    This composed operation sends the close frame if it hasn't already
    been sent, then reads and discards frames until receiving a close
    frame. Finally it invokes the teardown operation to shut down the
    underlying connection.
*/
template<class NextLayer, bool deflateSupported>
template<class Handler>
class stream<NextLayer, deflateSupported>::close_op
    : public beast::stable_async_op_base<
        Handler, beast::executor_type<stream>>
    , public net::coroutine
{
    stream<NextLayer, deflateSupported>& ws_;
    error_code ev_;
    detail::frame_buffer& fb_;

public:
    static constexpr int id = 4; // for soft_mutex

    template<class Handler_>
    close_op(
        Handler_&& h,
        stream<NextLayer, deflateSupported>& ws,
        close_reason const& cr)
        : stable_async_op_base<
            Handler, beast::executor_type<stream>>(
                std::forward<Handler_>(h), ws.get_executor())
        , ws_(ws)
        , fb_(beast::allocate_stable<detail::frame_buffer>(*this))
    {
        // Serialize the close frame
        ws.template write_close<
            flat_static_buffer_base>(fb_, cr);
        (*this)({}, 0, false);
    }

    void
    operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0,
        bool cont = true)
    {
        using beast::detail::clamp;
        auto& impl = *ws_.impl_;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            // Acquire the write lock
            if(! impl.wr_block.try_lock(this))
            {
                BOOST_ASIO_CORO_YIELD
                impl.paused_close.emplace(std::move(*this));
                impl.wr_block.lock(this);
                BOOST_ASIO_CORO_YIELD
                net::post(std::move(*this));
                BOOST_ASSERT(impl.wr_block.is_locked(this));
            }
            if(impl.check_stop_now(ec))
                goto upcall;

            // Can't call close twice
            // TODO return a custom error code
            BOOST_ASSERT(! impl.wr_close);

            // Send close frame
            impl.wr_close = true;
            impl.change_status(status::closing);
            BOOST_ASIO_CORO_YIELD
            net::async_write(impl.stream, fb_.data(),
                beast::detail::bind_continuation(std::move(*this)));
            if(impl.check_stop_now(ec))
                goto upcall;

            if(impl.rd_close)
            {
                // This happens when the read_op gets a close frame
                // at the same time close_op is sending the close frame.
                // The read_op will be suspended on the write block.
                goto teardown;
            }

            // Acquire the read lock
            if(! impl.rd_block.try_lock(this))
            {
                BOOST_ASIO_CORO_YIELD
                impl.paused_r_close.emplace(std::move(*this));
                impl.rd_block.lock(this);
                BOOST_ASIO_CORO_YIELD
                net::post(std::move(*this));
                BOOST_ASSERT(impl.rd_block.is_locked(this));
                if(impl.check_stop_now(ec))
                    goto upcall;
                BOOST_ASSERT(! impl.rd_close);
            }

            // Read until a receiving a close frame
            // TODO There should be a timeout on this
            if(impl.rd_remain > 0)
                goto read_payload;
            for(;;)
            {
                // Read frame header
                while(! ws_.parse_fh(
                    impl.rd_fh, impl.rd_buf, ev_))
                {
                    if(ev_)
                        goto teardown;
                    BOOST_ASIO_CORO_YIELD
                    impl.stream.async_read_some(
                        impl.rd_buf.prepare(read_size(
                            impl.rd_buf, impl.rd_buf.max_size())),
                        beast::detail::bind_continuation(std::move(*this)));
                    impl.rd_buf.commit(bytes_transferred);
                    if(impl.check_stop_now(ec))
                        goto upcall;
                }
                if(detail::is_control(impl.rd_fh.op))
                {
                    // Discard ping or pong frame
                    if(impl.rd_fh.op != detail::opcode::close)
                    {
                        impl.rd_buf.consume(clamp(impl.rd_fh.len));
                        continue;
                    }

                    // Process close frame
                    // TODO Should we invoke the control callback?
                    BOOST_ASSERT(! impl.rd_close);
                    impl.rd_close = true;
                    auto const mb = buffers_prefix(
                        clamp(impl.rd_fh.len),
                        impl.rd_buf.data());
                    if(impl.rd_fh.len > 0 && impl.rd_fh.mask)
                        detail::mask_inplace(mb, impl.rd_key);
                    detail::read_close(impl.cr, mb, ev_);
                    if(ev_)
                        goto teardown;
                    impl.rd_buf.consume(clamp(impl.rd_fh.len));
                    goto teardown;
                }

            read_payload:
                // Discard message frame
                while(impl.rd_buf.size() < impl.rd_remain)
                {
                    impl.rd_remain -= impl.rd_buf.size();
                    impl.rd_buf.consume(impl.rd_buf.size());
                    BOOST_ASIO_CORO_YIELD
                    impl.stream.async_read_some(
                        impl.rd_buf.prepare(read_size(
                            impl.rd_buf, impl.rd_buf.max_size())),
                        beast::detail::bind_continuation(std::move(*this)));
                    impl.rd_buf.commit(bytes_transferred);
                    if(impl.check_stop_now(ec))
                        goto upcall;
                }
                BOOST_ASSERT(impl.rd_buf.size() >= impl.rd_remain);
                impl.rd_buf.consume(clamp(impl.rd_remain));
                impl.rd_remain = 0;
            }

        teardown:
            // Teardown
            BOOST_ASSERT(impl.wr_block.is_locked(this));
            using beast::websocket::async_teardown;
            BOOST_ASIO_CORO_YIELD
            async_teardown(impl.role, impl.stream,
                beast::detail::bind_continuation(std::move(*this)));
            BOOST_ASSERT(impl.wr_block.is_locked(this));
            if(ec == net::error::eof)
            {
                // Rationale:
                // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
                ec = {};
            }
            if(! ec)
                ec = ev_;
            if(ec)
                impl.change_status(status::failed);
            else
                impl.change_status(status::closed);
            impl.close();

        upcall:
            impl.wr_block.unlock(this);
            impl.rd_block.try_unlock(this)
                && impl.paused_r_rd.maybe_invoke();
            impl.paused_rd.maybe_invoke()
                || impl.paused_ping.maybe_invoke()
                || impl.paused_wr.maybe_invoke();
            this->invoke(cont, ec);
        }
    }
};

//------------------------------------------------------------------------------

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
close(close_reason const& cr)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    error_code ec;
    close(cr, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
close(close_reason const& cr, error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    using beast::detail::clamp;
    auto& impl = *impl_;
    ec = {};
    if(impl.check_stop_now(ec))
        return;
    BOOST_ASSERT(! impl.rd_close);

    // Can't call close twice
    // TODO return a custom error code
    BOOST_ASSERT(! impl.wr_close);

    // Send close frame
    {
        impl.wr_close = true;
        impl.change_status(status::closing);
        detail::frame_buffer fb;
        write_close<flat_static_buffer_base>(fb, cr);
        net::write(impl.stream, fb.data(), ec);
        if(impl.check_stop_now(ec))
            return;
    }

    // Read until a receiving a close frame
    error_code ev;
    if(impl.rd_remain > 0)
        goto read_payload;
    for(;;)
    {
        // Read frame header
        while(! parse_fh(
            impl.rd_fh, impl.rd_buf, ev))
        {
            if(ev)
            {
                // Protocol violation
                return do_fail(close_code::none, ev, ec);
            }
            impl.rd_buf.commit(impl.stream.read_some(
                impl.rd_buf.prepare(read_size(
                    impl.rd_buf, impl.rd_buf.max_size())), ec));
            if(impl.check_stop_now(ec))
                return;
        }

        if(detail::is_control(impl.rd_fh.op))
        {
            // Discard ping/pong frame
            if(impl.rd_fh.op != detail::opcode::close)
            {
                impl.rd_buf.consume(clamp(impl.rd_fh.len));
                continue;
            }

            // Handle close frame
            // TODO Should we invoke the control callback?
            BOOST_ASSERT(! impl.rd_close);
            impl.rd_close = true;
            auto const mb = buffers_prefix(
                clamp(impl.rd_fh.len),
                impl.rd_buf.data());
            if(impl.rd_fh.len > 0 && impl.rd_fh.mask)
                detail::mask_inplace(mb, impl.rd_key);
            detail::read_close(impl.cr, mb, ev);
            if(ev)
            {
                // Protocol violation
                return do_fail(close_code::none, ev, ec);
            }
            impl.rd_buf.consume(clamp(impl.rd_fh.len));
            break;
        }

    read_payload:
        // Discard message frame
        while(impl.rd_buf.size() < impl.rd_remain)
        {
            impl.rd_remain -= impl.rd_buf.size();
            impl.rd_buf.consume(impl.rd_buf.size());
            impl.rd_buf.commit(
                impl.stream.read_some(
                    impl.rd_buf.prepare(
                        read_size(
                            impl.rd_buf,
                            impl.rd_buf.max_size())),
                    ec));
            if(impl.check_stop_now(ec))
                return;
        }
        BOOST_ASSERT(
            impl.rd_buf.size() >= impl.rd_remain);
        impl.rd_buf.consume(clamp(impl.rd_remain));
        impl.rd_remain = 0;
    }
    // _Close the WebSocket Connection_
    do_fail(close_code::none, error::closed, ec);
    if(ec == error::closed)
        ec = {};
}

template<class NextLayer, bool deflateSupported>
template<class CloseHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    CloseHandler, void(error_code))
stream<NextLayer, deflateSupported>::
async_close(close_reason const& cr, CloseHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        CloseHandler, void(error_code));
    close_op<BOOST_ASIO_HANDLER_TYPE(
        CloseHandler, void(error_code))>(
            std::move(init.completion_handler), *this, cr);
    return init.result.get();
}

} // websocket
} // beast
} // boost

#endif
