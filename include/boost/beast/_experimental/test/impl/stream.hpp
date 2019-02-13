//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
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

    class lambda
    {
        state& s_;
        Buffers b_;
        Handler h_;
        net::executor_work_guard<ex2_type> wg2_;

    public:
        lambda(lambda&&) = default;
        lambda(lambda const&) = default;

        template<class DeducedHandler>
        lambda(state& s, Buffers const& b, DeducedHandler&& h)
            : s_(s)
            , b_(b)
            , h_(std::forward<DeducedHandler>(h))
            , wg2_(net::get_associated_executor(
                h_, s_.ioc.get_executor()))
        {
        }

        void
        operator()(bool cancel)
        {
            error_code ec;
            std::size_t bytes_transferred = 0;
            if(cancel)
            {
                ec = net::error::operation_aborted;
            }
            else
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
                else
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
    read_op(state& s,
        Buffers const& b, Handler_&& h)
        : fn_(s, b, std::forward<Handler_>(h))
        , wg1_(s.ioc.get_executor())
    {
    }

    void
    operator()(bool cancel) override
    {
        net::post(
            wg1_.get_executor(),
            bind_handler(
                std::move(fn_),
                cancel));
        wg1_.reset();
    }
};

//------------------------------------------------------------------------------

stream::
state::
state(
    net::io_context& ioc_,
    fail_count* fc_)
    : ioc(ioc_)
    , fc(fc_)
{
}

stream::
state::
~state()
{
    // cancel outstanding read
    if(op != nullptr)
        (*op)(true);
}

void
stream::
state::
notify_read()
{
    if(op)
    {
        auto op_ = std::move(op);
        op_->operator()();
    }
    else
    {
        cv.notify_all();
    }
}

//------------------------------------------------------------------------------
        
stream::
~stream()
{
    close();
}

stream::
stream(stream&& other)
{
    auto in = std::make_shared<state>(
        other.in_->ioc, other.in_->fc);
    in_ = std::move(other.in_);
    out_ = std::move(other.out_);
    other.in_ = in;
}

stream&
stream::
operator=(stream&& other)
{
    close();
    auto in = std::make_shared<state>(
        other.in_->ioc, other.in_->fc);
    in_ = std::move(other.in_);
    out_ = std::move(other.out_);
    other.in_ = in;
    return *this;
}

//------------------------------------------------------------------------------

stream::
stream(net::io_context& ioc)
    : in_(std::make_shared<state>(ioc, nullptr))
{
}

stream::
stream(
    net::io_context& ioc,
    fail_count& fc)
    : in_(std::make_shared<state>(ioc, &fc))
{
}

stream::
stream(
    net::io_context& ioc,
    string_view s)
    : in_(std::make_shared<state>(ioc, nullptr))
{
    in_->b.commit(net::buffer_copy(
        in_->b.prepare(s.size()),
        net::buffer(s.data(), s.size())));
}

stream::
stream(
    net::io_context& ioc,
    fail_count& fc,
    string_view s)
    : in_(std::make_shared<state>(ioc, &fc))
{
    in_->b.commit(net::buffer_copy(
        in_->b.prepare(s.size()),
        net::buffer(s.data(), s.size())));
}

void
stream::
connect(stream& remote)
{
    BOOST_ASSERT(! out_.lock());
    BOOST_ASSERT(! remote.out_.lock());
    out_ = remote.in_;
    remote.out_ = in_;
    in_->code = status::ok;
    remote.in_->code = status::ok;
}

string_view
stream::
str() const
{
    auto const bs = in_->b.data();
    if(buffer_size(bs) == 0)
        return {};
    auto const b = beast::buffers_front(bs);
    return {static_cast<char const*>(b.data()), b.size()};
}

void
stream::
append(string_view s)
{
    std::lock_guard<std::mutex> lock{in_->m};
    in_->b.commit(net::buffer_copy(
        in_->b.prepare(s.size()),
        net::buffer(s.data(), s.size())));
}

void
stream::
clear()
{
    std::lock_guard<std::mutex> lock{in_->m};
    in_->b.consume(in_->b.size());
}

void
stream::
close()
{
    // cancel outstanding read
    {
        std::unique_ptr<read_op_base> op;
        {
            std::lock_guard<std::mutex> lock(in_->m);
            in_->code = status::eof;
            op = std::move(in_->op);
        }
        if(op != nullptr)
            (*op)(true);
    }

    // disconnect
    {
        auto out = out_.lock();
        out_.reset();

        // notify peer
        if(out)
        {
            std::lock_guard<std::mutex> lock(out->m);
            if(out->code == status::ok)
            {
                out->code = status::eof;
                out->notify_read();
            }
        }
    }
}

void
stream::
close_remote()
{
    std::lock_guard<std::mutex> lock{in_->m};
    if(in_->code == status::ok)
    {
        in_->code = status::eof;
        in_->notify_read();
    }
}

template<class MutableBufferSequence>
std::size_t
stream::
read_some(MutableBufferSequence const& buffers)
{
    static_assert(net::is_mutable_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence requirements not met");
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
        "MutableBufferSequence requirements not met");

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
        "MutableBufferSequence requirements not met");

    BOOST_BEAST_HANDLER_INIT(
        ReadHandler, void(error_code, std::size_t));

    ++in_->nread;

    std::unique_lock<std::mutex> lock(in_->m);
    if(in_->op != nullptr)
        throw std::logic_error(
            "in_->op != nullptr");

    // test failure
    error_code ec;
    if(in_->fc && in_->fc->fail(ec))
    {
        net::post(
            in_->ioc.get_executor(),
            beast::bind_front_handler(
                std::move(init.completion_handler),
                ec, std::size_t{0}));
        return init.result.get();
    }

    // A request to read 0 bytes from a stream is a no-op.
    if(buffer_size(buffers) == 0)
    {
        lock.unlock();
        net::post(
            in_->ioc.get_executor(),
            beast::bind_front_handler(
                std::move(init.completion_handler),
                ec, std::size_t{0}));
        return init.result.get();
    }

    // deliver bytes before eof
    if(buffer_size(in_->b.data()) > 0)
    {
        auto n = net::buffer_copy(
            buffers, in_->b.data(), in_->read_max);
        in_->b.consume(n);
        lock.unlock();
        net::post(
            in_->ioc.get_executor(),
            beast::bind_front_handler(
                std::move(init.completion_handler),
                ec, n));
        return init.result.get();
    }

    // deliver error
    if(in_->code != status::ok)
    {
        lock.unlock();
        ec = net::error::eof;
        net::post(
            in_->ioc.get_executor(),
            beast::bind_front_handler(
                std::move(init.completion_handler),
                ec, std::size_t{0}));
        return init.result.get();
    }

    // complete when bytes available or closed
    in_->op.reset(new read_op<BOOST_ASIO_HANDLER_TYPE(
        ReadHandler, void(error_code, std::size_t)),
            MutableBufferSequence>{*in_, buffers,
                std::move(init.completion_handler)});
    return init.result.get();
}

template<class ConstBufferSequence>
std::size_t
stream::
write_some(ConstBufferSequence const& buffers)
{
    static_assert(net::is_const_buffer_sequence<
            ConstBufferSequence>::value,
        "ConstBufferSequence requirements not met");
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
        "ConstBufferSequence requirements not met");

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
async_write_some(ConstBufferSequence const& buffers,
    WriteHandler&& handler)
{
    static_assert(net::is_const_buffer_sequence<
            ConstBufferSequence>::value,
        "ConstBufferSequence requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        WriteHandler, void(error_code, std::size_t));

    ++in_->nwrite;

    // test failure
    error_code ec;
    if(in_->fc && in_->fc->fail(ec))
    {
        net::post(
            in_->ioc.get_executor(),
            beast::bind_front_handler(
                std::move(init.completion_handler),
                ec,
                std::size_t{0}));
        return init.result.get();
    }

    // A request to read 0 bytes from a stream is a no-op.
    if(buffer_size(buffers) == 0)
    {
        net::post(
            in_->ioc.get_executor(),
            beast::bind_front_handler(
                std::move(init.completion_handler),
                ec, std::size_t{0}));
        return init.result.get();
    }

    // connection closed
    auto out = out_.lock();
    if(! out)
    {
        net::post(
            in_->ioc.get_executor(),
            beast::bind_front_handler(
                std::move(init.completion_handler),
                net::error::connection_reset,
                std::size_t{0}));
        return init.result.get();
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
    net::post(
        in_->ioc.get_executor(),
        beast::bind_front_handler(
            std::move(init.completion_handler),
            error_code{}, n));

    return init.result.get();
}

void
teardown(
    websocket::role_type,
    stream& s,
    boost::system::error_code& ec)
{
    if( s.in_->fc &&
        s.in_->fc->fail(ec))
        return;

    s.close();

    if( s.in_->fc &&
        s.in_->fc->fail(ec))
        ec = net::error::eof;
    else
        ec = {};
}

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

stream
connect(stream& to)
{
    stream from{to.get_executor().context()};
    from.connect(to);
    return from;
}

void
connect(stream& s1, stream& s2)
{
    s1.connect(s2);
}

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
