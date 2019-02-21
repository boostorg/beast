//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_IMPL_ICY_STREAM_HPP
#define BOOST_BEAST_CORE_IMPL_ICY_STREAM_HPP

#include <boost/beast/core/async_op_base.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/buffer_size.hpp>
#include <boost/beast/core/buffers_adaptor.hpp>
#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/beast/core/dynamic_buffer_ref.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/core/detail/buffers_ref.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/assert.hpp>
#include <boost/throw_exception.hpp>
#include <algorithm>
#include <cstring>
#include <memory>
#include <utility>

namespace boost {
namespace beast {
namespace http {

namespace detail {

template<class MutableBuffers, class ConstBuffers>
void
buffer_shift(MutableBuffers const& out, ConstBuffers const& in)
{
    auto in_pos  = net::buffer_sequence_end(in);
    auto out_pos = net::buffer_sequence_end(out);
    auto const in_begin  = net::buffer_sequence_begin(in);
    auto const out_begin = net::buffer_sequence_begin(out);
    BOOST_ASSERT(buffer_size(in) == buffer_size(out));
    if(in_pos == in_begin || out_pos == out_begin)
        return;
    net::const_buffer cb{*--in_pos};
    net::mutable_buffer mb{*--out_pos};
    for(;;)
    {
        if(mb.size() >= cb.size())
        {
            std::memmove(
                static_cast<char*>(
                    mb.data()) + mb.size() - cb.size(),
                cb.data(),
                cb.size());
            mb = net::mutable_buffer{
                mb.data(), mb.size() - cb.size()};
            if(in_pos == in_begin)
                break;
            cb = *--in_pos;
        }
        else
        {
            std::memmove(
                mb.data(),
                static_cast<char const*>(
                    cb.data()) + cb.size() - mb.size(),
                mb.size());
            cb = net::const_buffer{
                cb.data(), cb.size() - mb.size()};
            if(out_pos == out_begin)
                break;
            mb = *--out_pos;
        }
    }
}

template<class FwdIt>
class match_icy
{
    bool& match_;

public:
    using result_type = std::pair<FwdIt, bool>;
    explicit
    match_icy(bool& b)
        : match_(b)
    {
    }

    result_type
    operator()(FwdIt first, FwdIt last) const
    {
        auto it = first;
        if(it == last)
            return {first, false};
        if(*it != 'I')
            return {last, true};
        if(++it == last)
            return {first, false};
        if(*it != 'C')
            return {last, true};
        if(++it == last)
            return {first, false};
        if(*it != 'Y')
            return {last, true};
        match_ = true;
        return {last, true};
    };
};

} // detail

template<class NextLayer>
struct icy_stream<NextLayer>::ops
{

template<class Buffers, class Handler>
class read_op
    : public beast::stable_async_op_base<Handler,
        beast::executor_type<icy_stream>>
    , public net::coroutine
{
    // VFALCO We need a stable reference to `b`
    //        to pass to asio's read functions.
    //
    // VFALCO Why did I do all this rigamarole?
    //        we only need 6 or 8 bytes at most,
    //        this should all just be tucked
    //        away into the icy_stream. We can
    //        simulate a dynamic buffer adaptor
    //        with one or two simple ints.
    struct data
    {
        icy_stream& s;
        buffers_adaptor<Buffers> b;
        bool match = false;

        data(
            icy_stream& s_,
            Buffers const& b_)
            : s(s_)
            , b(b_)
        {
        }
    };

    data& d_;

public:
    template<class Handler_>
    read_op(
        Handler_&& h,
        icy_stream& s,
        Buffers const& b)
        : stable_async_op_base<Handler,
            beast::executor_type<icy_stream>>(
                std::forward<Handler_>(h), s.get_executor())
        , d_(beast::allocate_stable<data>(*this, s, b))
    {
        (*this)({}, 0);
    }

    void
    operator()(
        boost::system::error_code ec,
        std::size_t bytes_transferred)
    {
        using iterator = net::buffers_iterator<
            typename beast::dynamic_buffer_ref_wrapper<
                buffers_adaptor<Buffers>>::const_buffers_type>;
        BOOST_ASIO_CORO_REENTER(*this)
        {
            if(d_.b.max_size() == 0)
            {
                BOOST_ASIO_CORO_YIELD
                net::post(d_.s.get_executor(),
                    beast::bind_handler(std::move(*this), ec, 0));
                goto upcall;
            }
            if(! d_.s.detect_)
            {
                if(d_.s.copy_ > 0)
                {
                    auto const n = net::buffer_copy(
                        d_.b.prepare(std::min<std::size_t>(
                            d_.s.copy_, d_.b.max_size())),
                        net::buffer(d_.s.buf_));
                    d_.b.commit(n);
                    d_.s.copy_ = static_cast<unsigned char>(
                        d_.s.copy_ - n);
                    if(d_.s.copy_ > 0)
                        std::memmove(
                            d_.s.buf_,
                            &d_.s.buf_[n],
                            d_.s.copy_);
                }
                if(d_.b.size() < d_.b.max_size())
                {
                    BOOST_ASIO_CORO_YIELD
                    d_.s.next_layer().async_read_some(
                        d_.b.prepare(d_.b.max_size() - d_.b.size()),
                        std::move(*this));
                    d_.b.commit(bytes_transferred);
                }
                bytes_transferred = d_.b.size();
                goto upcall;
            }

            d_.s.detect_ = false;
            if(d_.b.max_size() < 8)
            {
                BOOST_ASIO_CORO_YIELD
                net::async_read(
                    d_.s.next_layer(),
                    net::buffer(d_.s.buf_, 3),
                    std::move(*this));
                if(ec)
                    goto upcall;
                auto n = bytes_transferred;
                BOOST_ASSERT(n == 3);
                if(
                    d_.s.buf_[0] != 'I' ||
                    d_.s.buf_[1] != 'C' ||
                    d_.s.buf_[2] != 'Y')
                {
                    net::buffer_copy(
                        d_.b.value(),
                        net::buffer(d_.s.buf_, n));
                    if(d_.b.max_size() < 3)
                    {
                        d_.s.copy_ = static_cast<unsigned char>(
                            3 - d_.b.max_size());
                        std::memmove(
                            d_.s.buf_,
                            &d_.s.buf_[d_.b.max_size()],
                            d_.s.copy_);

                    }
                    bytes_transferred = (std::min)(
                        n, d_.b.max_size());
                    goto upcall;
                }
                d_.s.copy_ = static_cast<unsigned char>(
                    net::buffer_copy(
                        net::buffer(d_.s.buf_),
                        icy_stream::version() + d_.b.max_size()));
                bytes_transferred = net::buffer_copy(
                    d_.b.value(), icy_stream::version());
                goto upcall;
            }

            BOOST_ASIO_CORO_YIELD
            net::async_read_until(
                d_.s.next_layer(),
                beast::dynamic_buffer_ref(d_.b),
                detail::match_icy<iterator>(d_.match),
                std::move(*this));
            if(ec)
                goto upcall;
            {
                auto n = bytes_transferred;
                BOOST_ASSERT(n == d_.b.size());
                if(! d_.match)
                    goto upcall;
                if(d_.b.size() + 5 > d_.b.max_size())
                {
                    d_.s.copy_ = static_cast<unsigned char>(
                        n + 5 - d_.b.max_size());
                    std::copy(
                        net::buffers_begin(d_.b.value()) + n - d_.s.copy_,
                        net::buffers_begin(d_.b.value()) + n,
                        d_.s.buf_);
                    n = d_.b.max_size() - 5;
                }
                {
                    buffers_suffix<beast::detail::buffers_ref<
                        Buffers>> dest(
                            boost::in_place_init, d_.b.value());
                    dest.consume(5);
                    detail::buffer_shift(
                        beast::buffers_prefix(n, dest),
                        beast::buffers_prefix(n, d_.b.value()));
                    net::buffer_copy(d_.b.value(), icy_stream::version());
                    n += 5;
                    bytes_transferred = n;
                }
            }
        upcall:
            this->invoke_now(ec, bytes_transferred);
        }
    }
};

struct run_read_op
{
    template<class ReadHandler, class Buffers>
    void
    operator()(
        ReadHandler&& h,
        icy_stream& s,
        Buffers const& b)
    {
        // If you get an error on the following line it means
        // that your handler does not meet the documented type
        // requirements for the handler.

        static_assert(
            beast::detail::is_invocable<ReadHandler,
            void(error_code, std::size_t)>::value,
            "ReadHandler type requirements not met");

        read_op<
            Buffers,
            typename std::decay<ReadHandler>::type>(
                std::forward<ReadHandler>(h), s, b);
    }
};

};

//------------------------------------------------------------------------------

template<class NextLayer>
template<class... Args>
icy_stream<NextLayer>::
icy_stream(Args&&... args)
    : stream_(std::forward<Args>(args)...)
{
}

template<class NextLayer>
template<class MutableBufferSequence>
std::size_t
icy_stream<NextLayer>::
read_some(MutableBufferSequence const& buffers)
{
    static_assert(is_sync_read_stream<next_layer_type>::value,
        "SyncReadStream type requirements not met");
    static_assert(net::is_mutable_buffer_sequence<
        MutableBufferSequence>::value,
            "MutableBufferSequence type requirements not met");
    error_code ec;
    auto n = read_some(buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return n;
}

template<class NextLayer>
template<class MutableBufferSequence>
std::size_t
icy_stream<NextLayer>::
read_some(MutableBufferSequence const& buffers, error_code& ec)
{
    static_assert(is_sync_read_stream<next_layer_type>::value,
        "SyncReadStream type requirements not met");
    static_assert(net::is_mutable_buffer_sequence<
        MutableBufferSequence>::value,
            "MutableBufferSequence type requirements not met");
    using iterator = net::buffers_iterator<
        typename beast::dynamic_buffer_ref_wrapper<
            buffers_adaptor<MutableBufferSequence>>::const_buffers_type>;
    buffers_adaptor<MutableBufferSequence> b(buffers);
    if(b.max_size() == 0)
    {
        ec = {};
        return 0;
    }
    if(! detect_)
    {
        if(copy_ > 0)
        {
            auto const n = net::buffer_copy(
                b.prepare(std::min<std::size_t>(
                    copy_, b.max_size())),
                net::buffer(buf_));
            b.commit(n);
            copy_ = static_cast<unsigned char>(
                copy_ - n);
            if(copy_ > 0)
                std::memmove(
                    buf_,
                    &buf_[n],
                    copy_);
        }
        if(b.size() < b.max_size())
            b.commit(stream_.read_some(
                b.prepare(b.max_size() - b.size()), ec));
        return b.size();
    }

    detect_ = false;
    if(b.max_size() < 8)
    {
        auto n = net::read(
            stream_,
            net::buffer(buf_, 3),
            ec);
        if(ec)
            return 0;
        BOOST_ASSERT(n == 3);
        if(
            buf_[0] != 'I' ||
            buf_[1] != 'C' ||
            buf_[2] != 'Y')
        {
            net::buffer_copy(
                buffers,
                net::buffer(buf_, n));
            if(b.max_size() < 3)
            {
                copy_ = static_cast<unsigned char>(
                    3 - b.max_size());
                std::memmove(
                    buf_,
                    &buf_[b.max_size()],
                    copy_);

            }
            return std::min<std::size_t>(
                n, b.max_size());
        }
        copy_ = static_cast<unsigned char>(
            net::buffer_copy(
                net::buffer(buf_),
                version() + b.max_size()));
        return net::buffer_copy(
            buffers,
            version());
    }

    bool match = false;
    auto n = net::read_until(
        stream_,
        beast::dynamic_buffer_ref(b),
        detail::match_icy<iterator>(match),
        ec);
    if(ec)
        return n;
    BOOST_ASSERT(n == b.size());
    if(! match)
        return n;
    if(b.size() + 5 > b.max_size())
    {
        copy_ = static_cast<unsigned char>(
            n + 5 - b.max_size());
        std::copy(
            net::buffers_begin(buffers) + n - copy_,
            net::buffers_begin(buffers) + n,
            buf_);
        n = b.max_size() - 5;
    }
    buffers_suffix<beast::detail::buffers_ref<
        MutableBufferSequence>> dest(
            boost::in_place_init, buffers);
    dest.consume(5);
    detail::buffer_shift(
        buffers_prefix(n, dest),
        buffers_prefix(n, buffers));
    net::buffer_copy(buffers, version());
    n += 5;
    return n;
}

template<class NextLayer>
template<
    class MutableBufferSequence,
    class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
icy_stream<NextLayer>::
async_read_some(
    MutableBufferSequence const& buffers,
    ReadHandler&& handler)
{
    static_assert(is_async_read_stream<next_layer_type>::value,
        "AsyncReadStream type requirements not met");
    static_assert(net::is_mutable_buffer_sequence<
            MutableBufferSequence >::value,
        "MutableBufferSequence type requirements not met");
    return net::async_initiate<
        ReadHandler,
        void(error_code, std::size_t)>(
            typename ops::run_read_op{},
            handler,
            *this,
            buffers);
}

template<class NextLayer>
template<class MutableBufferSequence>
std::size_t
icy_stream<NextLayer>::
write_some(MutableBufferSequence const& buffers)
{
    static_assert(is_sync_write_stream<next_layer_type>::value,
        "SyncWriteStream type requirements not met");
    static_assert(net::is_const_buffer_sequence<
        MutableBufferSequence>::value,
            "MutableBufferSequence type requirements not met");
    return stream_.write_some(buffers);
}

template<class NextLayer>
template<class MutableBufferSequence>
std::size_t
icy_stream<NextLayer>::
write_some(MutableBufferSequence const& buffers, error_code& ec)
{
    static_assert(is_sync_write_stream<next_layer_type>::value,
        "SyncWriteStream type requirements not met");
    static_assert(net::is_const_buffer_sequence<
        MutableBufferSequence>::value,
            "MutableBufferSequence type requirements not met");
    return stream_.write_some(buffers, ec);
}

template<class NextLayer>
template<
    class MutableBufferSequence,
    class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    WriteHandler, void(error_code, std::size_t))
icy_stream<NextLayer>::
async_write_some(
    MutableBufferSequence const& buffers,
    WriteHandler&& handler)
{
    static_assert(is_async_write_stream<next_layer_type>::value,
        "AsyncWriteStream type requirements not met");
    static_assert(net::is_const_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence type requirements not met");
    return stream_.async_write_some(
        buffers, std::forward<WriteHandler>(handler));
}

} // http
} // beast
} // boost

#endif
