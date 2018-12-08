//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_IMPL_ICY_STREAM_HPP
#define BOOST_BEAST_CORE_IMPL_ICY_STREAM_HPP

#include <boost/beast/_experimental/core/detail/dynamic_buffer_ref.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/buffers_adapter.hpp>
#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/beast/core/detail/buffers_ref.hpp>
#include <boost/beast/core/detail/stream_algorithm.hpp>
#include <boost/beast/core/handler_ptr.hpp>
#include <boost/asio/buffers_iterator.hpp>
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
    using net::buffer_size;
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
template<class MutableBufferSequence, class Handler>
class icy_stream<NextLayer>::read_op
    : public net::coroutine
{
    using alloc_type = typename
#if defined(BOOST_NO_CXX11_ALLOCATOR)
        net::associated_allocator_t<Handler>::template
            rebind<char>::other;
#else
        std::allocator_traits<net::associated_allocator_t<
            Handler>>::template rebind_alloc<char>;
#endif

    struct data
    {
        icy_stream<NextLayer>& s;
        buffers_adapter<MutableBufferSequence> b;
        bool match = false;

        data(
            Handler const&,
            icy_stream<NextLayer>& s_,
            MutableBufferSequence const& b_)
            : s(s_)
            , b(b_)
        {
        }
    };

    handler_ptr<data, Handler> d_;

public:
    read_op(read_op&&) = default;
    read_op(read_op const&) = delete;

    template<class DeducedHandler, class... Args>
    read_op(
        DeducedHandler&& h,
        icy_stream<NextLayer>& s,
        MutableBufferSequence const& b)
        : d_(std::forward<DeducedHandler>(h), s, b)
    {
    }

    void
    operator()(
        boost::system::error_code ec,
        std::size_t bytes_transferred);

    //

    using allocator_type =
        net::associated_allocator_t<Handler>;

    using executor_type = net::associated_executor_t<
        Handler, decltype(std::declval<NextLayer&>().get_executor())>;

    allocator_type
    get_allocator() const noexcept
    {
        return net::get_associated_allocator(d_.handler());
    }

    executor_type
    get_executor() const noexcept
    {
        return net::get_associated_executor(
            d_.handler(), d_->s.get_executor());
    }

    template<class Function>
    friend
    void asio_handler_invoke(
        Function&& f, read_op* op)
    {
        using net::asio_handler_invoke;
        asio_handler_invoke(
            f, std::addressof(op->d_.handler()));
    }

    friend
    void* asio_handler_allocate(
        std::size_t size, read_op* op)
    {
        using net::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(op->d_.handler()));
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, read_op* op)
    {
        using net::asio_handler_deallocate;
        asio_handler_deallocate(
            p, size, std::addressof(op->d_.handler()));
    }

    friend
    bool asio_handler_is_continuation(read_op* op)
    {
        using net::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->d_.handler()));
    }
};

template<class NextLayer>
template<class MutableBufferSequence, class Handler>
void
icy_stream<NextLayer>::
read_op<MutableBufferSequence, Handler>::
operator()(
    error_code ec,
    std::size_t bytes_transferred)
{
    using iterator = net::buffers_iterator<
        typename beast::detail::dynamic_buffer_ref<
            buffers_adapter<MutableBufferSequence>>::const_buffers_type>;
    auto& d = *d_;
    BOOST_ASIO_CORO_REENTER(*this)
    {
        if(d.b.max_size() == 0)
        {
            BOOST_ASIO_CORO_YIELD
            net::post(d.s.get_executor(),
                beast::bind_handler(std::move(*this), ec, 0));
            goto upcall;
        }
        if(! d.s.detect_)
        {
            if(d.s.copy_ > 0)
            {
                auto const n = net::buffer_copy(
                    d.b.prepare(std::min<std::size_t>(
                        d.s.copy_, d.b.max_size())),
                    net::buffer(d.s.buf_));
                d.b.commit(n);
                d.s.copy_ = static_cast<unsigned char>(
                    d.s.copy_ - n);
                if(d.s.copy_ > 0)
                    std::memmove(
                        d.s.buf_,
                        &d.s.buf_[n],
                        d.s.copy_);
            }
            if(d.b.size() < d.b.max_size())
            {
                BOOST_ASIO_CORO_YIELD
                d.s.next_layer().async_read_some(
                    d.b.prepare(d.b.max_size() - d.b.size()),
                    std::move(*this));
                d.b.commit(bytes_transferred);
            }
            bytes_transferred = d.b.size();
            goto upcall;
        }

        d.s.detect_ = false;
        if(d.b.max_size() < 8)
        {
            BOOST_ASIO_CORO_YIELD
            net::async_read(
                d.s.next_layer(),
                net::buffer(d.s.buf_, 3),
                std::move(*this));
            if(ec)
                goto upcall;
            auto n = bytes_transferred;
            BOOST_ASSERT(n == 3);
            if(
                d.s.buf_[0] != 'I' ||
                d.s.buf_[1] != 'C' ||
                d.s.buf_[2] != 'Y')
            {
                net::buffer_copy(
                    d.b.value(),
                    net::buffer(d.s.buf_, n));
                if(d.b.max_size() < 3)
                {
                    d.s.copy_ = static_cast<unsigned char>(
                        3 - d.b.max_size());
                    std::memmove(
                        d.s.buf_,
                        &d.s.buf_[d.b.max_size()],
                        d.s.copy_);

                }
                bytes_transferred = (std::min)(
                    n, d.b.max_size());
                goto upcall;
            }
            d.s.copy_ = static_cast<unsigned char>(
                buffer_copy(
                    net::buffer(d.s.buf_),
                    icy_stream::version() + d.b.max_size()));
            bytes_transferred = buffer_copy(
                d.b.value(),
                icy_stream::version());
            goto upcall;
        }

        BOOST_ASIO_CORO_YIELD
        net::async_read_until(
            d.s.next_layer(),
            beast::detail::ref(d.b),
            detail::match_icy<iterator>(d.match),
            std::move(*this));
        if(ec)
            goto upcall;
        {
            auto n = bytes_transferred;
            BOOST_ASSERT(n == d.b.size());
            if(! d.match)
                goto upcall;
            if(d.b.size() + 5 > d.b.max_size())
            {
                d.s.copy_ = static_cast<unsigned char>(
                    n + 5 - d.b.max_size());
                std::copy(
                    net::buffers_begin(d.b.value()) + n - d.s.copy_,
                    net::buffers_begin(d.b.value()) + n,
                    d.s.buf_);
                n = d.b.max_size() - 5;
            }
            {
                buffers_suffix<beast::detail::buffers_ref<
                    MutableBufferSequence>> dest(
                        boost::in_place_init, d.b.value());
                dest.consume(5);
                detail::buffer_shift(
                    beast::buffers_prefix(n, dest),
                    beast::buffers_prefix(n, d.b.value()));
                net::buffer_copy(d.b.value(), icy_stream::version());
                n += 5;
                bytes_transferred = n;
            }
        }
    upcall:
        d_.invoke(ec, bytes_transferred);
    }
}

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
        "SyncReadStream requirements not met");
    static_assert(net::is_mutable_buffer_sequence<
        MutableBufferSequence>::value,
            "MutableBufferSequence requirements not met");
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
        "SyncReadStream requirements not met");
    static_assert(net::is_mutable_buffer_sequence<
        MutableBufferSequence>::value,
            "MutableBufferSequence requirements not met");
    using iterator = net::buffers_iterator<
        typename beast::detail::dynamic_buffer_ref<
            buffers_adapter<MutableBufferSequence>>::const_buffers_type>;
    buffers_adapter<MutableBufferSequence> b(buffers);
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
        beast::detail::ref(b),
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
        "AsyncReadStream requirements not met");
    static_assert(net::is_mutable_buffer_sequence<
            MutableBufferSequence >::value,
        "MutableBufferSequence  requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        ReadHandler, void(error_code, std::size_t));
    read_op<
        MutableBufferSequence,
        BOOST_ASIO_HANDLER_TYPE(
            ReadHandler, void(error_code, std::size_t))>{
                std::move(init.completion_handler), *this, buffers}(
                    {}, 0);
    return init.result.get();
}

template<class NextLayer>
template<class MutableBufferSequence>
std::size_t
icy_stream<NextLayer>::
write_some(MutableBufferSequence const& buffers)
{
    static_assert(is_sync_write_stream<next_layer_type>::value,
        "SyncWriteStream requirements not met");
    static_assert(net::is_const_buffer_sequence<
        MutableBufferSequence>::value,
            "MutableBufferSequence requirements not met");
    return stream_.write_some(buffers);
}

template<class NextLayer>
template<class MutableBufferSequence>
std::size_t
icy_stream<NextLayer>::
write_some(MutableBufferSequence const& buffers, error_code& ec)
{
    static_assert(is_sync_write_stream<next_layer_type>::value,
        "SyncWriteStream requirements not met");
    static_assert(net::is_const_buffer_sequence<
        MutableBufferSequence>::value,
            "MutableBufferSequence requirements not met");
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
        "AsyncWriteStream requirements not met");
    static_assert(net::is_const_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence requirements not met");
    return stream_.async_write_some(
        buffers, std::forward<WriteHandler>(handler));
}

} // http
} // beast
} // boost

#endif
