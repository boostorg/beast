//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_READ_IPP_HPP
#define BEAST_HTTP_IMPL_READ_IPP_HPP

#include <beast/http/concepts.hpp>
#include <beast/http/parse.hpp>
#include <beast/http/parser_v1.hpp>
#include <beast/core/bind_handler.hpp>
#include <beast/core/handler_alloc.hpp>
#include <beast/core/stream_concepts.hpp>
#include <boost/assert.hpp>

namespace beast {
namespace http {

namespace detail {

template<class Stream, class DynamicBuffer,
    bool isRequest, class Body, class Headers,
        class Handler>
class read_op
{
    using alloc_type =
        handler_alloc<char, Handler>;

    using parser_type =
        parser_v1<isRequest, Body, Headers>;

    using message_type =
        message<isRequest, Body, Headers>;

    struct data
    {
        Stream& s;
        DynamicBuffer& db;
        message_type& m;
        parser_type p;
        Handler h;
        bool started = false;
        bool cont;
        int state = 0;

        template<class DeducedHandler>
        data(DeducedHandler&& h_, Stream& s_,
                DynamicBuffer& sb_, message_type& m_)
            : s(s_)
            , db(sb_)
            , m(m_)
            , h(std::forward<DeducedHandler>(h_))
            , cont(boost_asio_handler_cont_helpers::
                is_continuation(h))
        {
        }
    };

    std::shared_ptr<data> d_;

public:
    read_op(read_op&&) = default;
    read_op(read_op const&) = default;

    template<class DeducedHandler, class... Args>
    read_op(DeducedHandler&& h, Stream& s, Args&&... args)
        : d_(std::allocate_shared<data>(alloc_type{h},
            std::forward<DeducedHandler>(h), s,
                std::forward<Args>(args)...))
    {
        (*this)(error_code{}, false);
    }

    void
    operator()(error_code ec, bool again = true);

    friend
    void* asio_handler_allocate(
        std::size_t size, read_op* op)
    {
        return boost_asio_handler_alloc_helpers::
            allocate(size, op->d_->h);
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, read_op* op)
    {
        return boost_asio_handler_alloc_helpers::
            deallocate(p, size, op->d_->h);
    }

    friend
    bool asio_handler_is_continuation(read_op* op)
    {
        return op->d_->cont;
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, read_op* op)
    {
        return boost_asio_handler_invoke_helpers::
            invoke(f, op->d_->h);
    }
};

template<class Stream, class DynamicBuffer,
    bool isRequest, class Body, class Headers,
        class Handler>
void
read_op<Stream, DynamicBuffer, isRequest, Body, Headers, Handler>::
operator()(error_code ec, bool again)
{
    auto& d = *d_;
    d.cont = d.cont || again;
    while(! ec && d.state != 99)
    {
        switch(d.state)
        {
        case 0:
            d.state = 1;
            async_parse(d.s, d.db, d.p, std::move(*this));
            return;

        case 1:
            // call handler
            d.state = 99;
            d.m = d.p.release();
            break;
        }
    }
    d.h(ec);
}

} // detail

//------------------------------------------------------------------------------

template<class SyncReadStream, class DynamicBuffer,
    bool isRequest, class Body, class Headers>
void
read(SyncReadStream& stream, DynamicBuffer& dynabuf,
    message<isRequest, Body, Headers>& msg)
{
    static_assert(is_SyncReadStream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(is_Body<Body>::value,
        "Body requirements not met");
    static_assert(has_reader<Body>::value,
        "Body has no reader");
    static_assert(is_Reader<typename Body::reader,
        message<isRequest, Body, Headers>>::value,
            "Reader requirements not met");
    error_code ec;
    beast::http::read(stream, dynabuf, msg, ec);
    if(ec)
        throw system_error{ec};
}

template<class SyncReadStream, class DynamicBuffer,
    bool isRequest, class Body, class Headers>
void
read(SyncReadStream& stream, DynamicBuffer& dynabuf,
    message<isRequest, Body, Headers>& m,
        error_code& ec)
{
    static_assert(is_SyncReadStream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(is_Body<Body>::value,
        "Body requirements not met");
    static_assert(has_reader<Body>::value,
        "Body has no reader");
    static_assert(is_Reader<typename Body::reader,
        message<isRequest, Body, Headers>>::value,
            "Reader requirements not met");
    parser_v1<isRequest, Body, Headers> p;
    beast::http::parse(stream, dynabuf, p, ec);
    if(ec)
        return;
    BOOST_ASSERT(p.complete());
    m = p.release();
}

template<class AsyncReadStream, class DynamicBuffer,
    bool isRequest, class Body, class Headers,
        class ReadHandler>
typename async_completion<
    ReadHandler, void(error_code)>::result_type
async_read(AsyncReadStream& stream, DynamicBuffer& dynabuf,
    message<isRequest, Body, Headers>& m,
        ReadHandler&& handler)
{
    static_assert(is_AsyncReadStream<AsyncReadStream>::value,
        "AsyncReadStream requirements not met");
    static_assert(is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(is_Body<Body>::value,
        "Body requirements not met");
    static_assert(has_reader<Body>::value,
        "Body has no reader");
    static_assert(is_Reader<typename Body::reader,
        message<isRequest, Body, Headers>>::value,
            "Reader requirements not met");
    beast::async_completion<ReadHandler,
        void(error_code)> completion(handler);
    detail::read_op<AsyncReadStream, DynamicBuffer,
        isRequest, Body, Headers, decltype(
            completion.handler)>{completion.handler,
                stream, dynabuf, m};
    return completion.result.get();
}

} // http
} // beast

#endif
