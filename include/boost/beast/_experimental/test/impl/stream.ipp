//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_IMPL_STREAM_IPP
#define BOOST_BEAST_TEST_IMPL_STREAM_IPP

#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/buffer_size.hpp>
#include <boost/beast/core/buffers_prefix.hpp>
#include <stdexcept>

namespace boost {
namespace beast {
namespace test {

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

//------------------------------------------------------------------------------

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

} // test
} // beast
} // boost

#endif
