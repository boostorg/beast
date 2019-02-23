//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_IMPL_STREAM_HPP
#define BOOST_BEAST_TEST_IMPL_STREAM_HPP

#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/buffer_size.hpp>
#include <boost/beast/core/buffers_prefix.hpp>
#include <stdexcept>

namespace boost {
namespace beast {
namespace test {

//------------------------------------------------------------------------------

template<class Handler, class Buffers>
class stream::read_op : public stream::read_op_base
{
    using ex1_type =
        net::io_context::executor_type;
    using ex2_type
        = net::associated_executor_t<Handler, ex1_type>;

    struct lambda
    {
        Handler h_;
        state& s_;
        Buffers b_;
        net::executor_work_guard<ex2_type> wg2_;

        lambda(lambda&&) = default;
        lambda(lambda const&) = default;

        template<class Handler_>
        lambda(
            Handler_&& h,
            state& s,
            Buffers const& b)
            : h_(std::forward<Handler_>(h))
            , s_(s)
            , b_(b)
            , wg2_(net::get_associated_executor(
                h_, s_.ioc.get_executor()))
        {
        }

        void
        operator()(error_code ec)
        {
            std::size_t bytes_transferred = 0;
            if (!ec)
            {
                std::lock_guard<std::mutex> lock(s_.m);
                BOOST_ASSERT(! s_.op);
                if(s_.b.size() > 0)
                {
                    bytes_transferred =
                        net::buffer_copy(
                            b_, s_.b.data(), s_.read_max);
                    s_.b.consume(bytes_transferred);
                }
                else if (buffer_size(b_) > 0)
                {
                    ec = net::error::eof;
                }
            }

            auto alloc = net::get_associated_allocator(h_);
            wg2_.get_executor().dispatch(
                beast::bind_front_handler(std::move(h_),
                    ec, bytes_transferred), alloc);
            wg2_.reset();
        }
    };

    lambda fn_;
    net::executor_work_guard<ex1_type> wg1_;

public:
    template<class Handler_>
    read_op(
        Handler_&& h,
        state& s,
        Buffers const& b)
        : fn_(std::forward<Handler_>(h), s, b)
        , wg1_(s.ioc.get_executor())
    {
    }

    void
    operator()(error_code ec) override
    {

        auto alloc = net::get_associated_allocator(fn_.h_);
        wg1_.get_executor().post(
            beast::bind_front_handler(std::move(fn_), ec), alloc);
        wg1_.reset();
    }
};

struct stream::run_read_op
{
    template<
        class ReadHandler,
        class MutableBufferSequence>
    void
    operator()(
        ReadHandler&& h,
        stream& s,
        MutableBufferSequence const& buffers)
    {
        // If you get an error on the following line it means
        // that your handler does not meet the documented type
        // requirements for the handler.

        static_assert(
            beast::detail::is_invocable<ReadHandler,
                void(error_code, std::size_t)>::value,
            "ReadHandler type requirements not met");

        s.initiate_read(
            std::unique_ptr<read_op_base>{
            new read_op<
                typename std::decay<ReadHandler>::type,
                MutableBufferSequence>(
                    std::move(h),
                    *s.in_,
                    buffers)},
            buffer_size(buffers));
    }
};

struct stream::run_write_op
{
    template<
        class WriteHandler,
        class ConstBufferSequence>
    void
    operator()(
        WriteHandler&& h,
        std::shared_ptr<state> in_,
        std::weak_ptr<state> out_,
        ConstBufferSequence const& buffers)
    {
        // If you get an error on the following line it means
        // that your handler does not meet the documented type
        // requirements for the handler.

        static_assert(
            beast::detail::is_invocable<WriteHandler,
                void(error_code, std::size_t)>::value,
            "WriteHandler type requirements not met");

        ++in_->nwrite;
        auto const complete_op = [&](error_code ec, std::size_t n)
        {
            net::post(
                in_->ioc.get_executor(),
                beast::bind_front_handler(std::move(h), ec, n));
        };

        // test failure
        error_code ec;
        std::size_t n = 0;
        if(in_->fc && in_->fc->fail(ec))
        {
            return complete_op(ec, n);
        }

        // A request to write 0 bytes to a stream is a no-op.
        if(buffer_size(buffers) == 0)
        {
            return complete_op(ec, n);
        }

        // connection closed
        auto out = out_.lock();
        if(! out)
        {
            return complete_op(net::error::connection_reset, n);
        }

        // copy buffers
        n = std::min<std::size_t>(
            buffer_size(buffers), in_->write_max);
        {
            std::lock_guard<std::mutex> lock(out->m);
            n = net::buffer_copy(out->b.prepare(n), buffers);
            out->b.commit(n);
            out->notify_read();
        }
        BOOST_ASSERT(! ec);
        complete_op(ec, n);
    }
};

//------------------------------------------------------------------------------

template<class MutableBufferSequence>
std::size_t
stream::
read_some(MutableBufferSequence const& buffers)
{
    static_assert(net::is_mutable_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence type requirements not met");
    error_code ec;
    auto const n = read_some(buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return n;
}

template<class MutableBufferSequence>
std::size_t
stream::
read_some(MutableBufferSequence const& buffers,
    error_code& ec)
{
    static_assert(net::is_mutable_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence type requirements not met");

    ++in_->nread;

    // test failure
    if(in_->fc && in_->fc->fail(ec))
        return 0;

    // A request to read 0 bytes from a stream is a no-op.
    if(buffer_size(buffers) == 0)
    {
        ec = {};
        return 0;
    }

    std::unique_lock<std::mutex> lock{in_->m};
    BOOST_ASSERT(! in_->op);
    in_->cv.wait(lock,
        [&]()
        {
            return
                in_->b.size() > 0 ||
                in_->code != status::ok;
        });

    // deliver bytes before eof
    if(in_->b.size() > 0)
    {
        auto const n = net::buffer_copy(
            buffers, in_->b.data(), in_->read_max);
        in_->b.consume(n);
        return n;
    }

    // deliver error
    BOOST_ASSERT(in_->code != status::ok);
    ec = net::error::eof;
    return 0;
}

template<class MutableBufferSequence, class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
stream::
async_read_some(
    MutableBufferSequence const& buffers,
    ReadHandler&& handler)
{
    static_assert(net::is_mutable_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence type requirements not met");

    return net::async_initiate<
        ReadHandler,
        void(error_code, std::size_t)>(
            run_read_op{},
            handler,
            *this,
            buffers);
}

template<class ConstBufferSequence>
std::size_t
stream::
write_some(ConstBufferSequence const& buffers)
{
    static_assert(net::is_const_buffer_sequence<
            ConstBufferSequence>::value,
        "ConstBufferSequence type requirements not met");
    error_code ec;
    auto const bytes_transferred =
        write_some(buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<class ConstBufferSequence>
std::size_t
stream::
write_some(
    ConstBufferSequence const& buffers, error_code& ec)
{
    static_assert(net::is_const_buffer_sequence<
            ConstBufferSequence>::value,
        "ConstBufferSequence type requirements not met");

    ++in_->nwrite;

    // test failure
    if(in_->fc && in_->fc->fail(ec))
        return 0;

    // A request to write 0 bytes to a stream is a no-op.
    if(buffer_size(buffers) == 0)
    {
        ec = {};
        return 0;
    }

    // connection closed
    auto out = out_.lock();
    if(! out)
    {
        ec = net::error::connection_reset;
        return 0;
    }

    // copy buffers
    auto n = std::min<std::size_t>(
        buffer_size(buffers), in_->write_max);
    {
        std::lock_guard<std::mutex> lock(out->m);
        n = net::buffer_copy(out->b.prepare(n), buffers);
        out->b.commit(n);
        out->notify_read();
    }
    return n;
}

template<class ConstBufferSequence, class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    WriteHandler, void(error_code, std::size_t))
stream::
async_write_some(
    ConstBufferSequence const& buffers,
    WriteHandler&& handler)
{
    static_assert(net::is_const_buffer_sequence<
            ConstBufferSequence>::value,
        "ConstBufferSequence type requirements not met");

    return net::async_initiate<
        WriteHandler,
        void(error_code, std::size_t)>(
            run_write_op{},
            handler,
            in_,
            out_,
            buffers);
}

//------------------------------------------------------------------------------

template<class TeardownHandler>
void
async_teardown(
    websocket::role_type,
    stream& s,
    TeardownHandler&& handler)
{
    error_code ec;
    if( s.in_->fc &&
        s.in_->fc->fail(ec))
        return net::post(
            s.get_executor(),
            beast::bind_front_handler(
                std::move(handler), ec));
    s.close();
    if( s.in_->fc &&
        s.in_->fc->fail(ec))
        ec = net::error::eof;
    else
        ec = {};

    net::post(
        s.get_executor(),
        beast::bind_front_handler(
            std::move(handler), ec));
}

//------------------------------------------------------------------------------

template<class Arg1, class... ArgN>
stream
connect(stream& to, Arg1&& arg1, ArgN&&... argn)
{
    stream from{
        std::forward<Arg1>(arg1),
        std::forward<ArgN>(argn)...};
    from.connect(to);
    return from;
}

} // test
} // beast
} // boost

#endif
