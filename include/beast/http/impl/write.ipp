//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_WRITE_IPP
#define BEAST_HTTP_IMPL_WRITE_IPP

#include <beast/http/type_traits.hpp>
#include <beast/core/bind_handler.hpp>
#include <beast/core/ostream.hpp>
#include <beast/core/handler_alloc.hpp>
#include <beast/core/handler_ptr.hpp>
#include <beast/core/type_traits.hpp>
#include <beast/core/detail/sync_ostream.hpp>
#include <boost/asio/handler_alloc_hook.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/optional.hpp>
#include <boost/throw_exception.hpp>
#include <ostream>
#include <sstream>

namespace beast {
namespace http {
namespace detail {

template<
    class Stream, class Serializer, class Handler>
class write_some_op
{
    Stream& s_;
    Serializer& sr_;
    Handler h_;

    class lambda
    {
        write_some_op& op_;

    public:
        bool empty = true;

        explicit
        lambda(write_some_op& op)
            : op_(op)
        {
        }

        template<class ConstBufferSequence>
        void
        operator()(error_code& ec,
            ConstBufferSequence const& buffer)
        {
            empty = false;
            return op_.s_.async_write_some(
                buffer, std::move(op_));
        }
    };

public:
    write_some_op(write_some_op&&) = default;
    write_some_op(write_some_op const&) = default;

    template<class DeducedHandler>
    write_some_op(DeducedHandler&& h,
            Stream& s, Serializer& sr)
        : s_(s)
        , sr_(sr)
        , h_(std::forward<DeducedHandler>(h))
    {
    }

    void
    operator()();

    void
    operator()(error_code ec,
        std::size_t bytes_transferred);

    friend
    void* asio_handler_allocate(
        std::size_t size, write_some_op* op)
    {
        using boost::asio::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(op->h_));
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, write_some_op* op)
    {
        using boost::asio::asio_handler_deallocate;
        asio_handler_deallocate(
            p, size, std::addressof(op->h_));
    }

    friend
    bool asio_handler_is_continuation(write_some_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, write_some_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(op->h_));
    }
};

template<
    class Stream, class Serializer, class Handler>
void
write_some_op<Stream, Serializer, Handler>::
operator()()
{
    lambda f{*this};
    error_code ec;
    sr_.get(ec, f);
    if(! ec && ! f.empty)
        return;
    return s_.get_io_service().post(
        bind_handler(std::move(*this), ec, 0));
}

template<
    class Stream, class Serializer, class Handler>
void
write_some_op<Stream, Serializer, Handler>::
operator()(error_code ec, std::size_t bytes_transferred)
{
    if(! ec)
        sr_.consume(bytes_transferred);
    h_(ec);
}

//------------------------------------------------------------------------------

template<class Stream, class Handler,
    bool isRequest, class Body, class Fields>
class write_op
{
    struct data
    {
        int state = 0;
        Stream& s;
        serializer<isRequest, Body, Fields,
            empty_decorator, handler_alloc<char, Handler>> sr;

        data(Handler& h, Stream& s_, message<
                isRequest, Body, Fields> const& m_)
            : s(s_)
            , sr(m_, empty_decorator{},
                handler_alloc<char, Handler>{h})
        {
        }
    };

    handler_ptr<data, Handler> d_;

public:
    write_op(write_op&&) = default;
    write_op(write_op const&) = default;

    template<class DeducedHandler, class... Args>
    write_op(DeducedHandler&& h, Stream& s, Args&&... args)
        : d_(std::forward<DeducedHandler>(h),
            s, std::forward<Args>(args)...)
    {
    }

    void
    operator()(error_code ec);

    friend
    void* asio_handler_allocate(
        std::size_t size, write_op* op)
    {
        using boost::asio::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(op->d_.handler()));
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, write_op* op)
    {
        using boost::asio::asio_handler_deallocate;
        asio_handler_deallocate(
            p, size, std::addressof(op->d_.handler()));
    }

    friend
    bool asio_handler_is_continuation(write_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return op->d_->state == 2 ||
            asio_handler_is_continuation(
                std::addressof(op->d_.handler()));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, write_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(
            f, std::addressof(op->d_.handler()));
    }
};

template<class Stream, class Handler,
    bool isRequest, class Body, class Fields>
void
write_op<Stream, Handler, isRequest, Body, Fields>::
operator()(error_code ec)
{
    auto& d = *d_;
    if(ec)
        goto upcall;    
    switch(d.state)
    {
    case 0:
        d.state = 1;
        write_some_op<Stream, decltype(d.sr),
            write_op>{std::move(*this), d.s, d.sr}();
        return;
    case 1:
        d.state = 2;
        // [[fallthrough]]
    case 2:
        if(d.sr.is_done())
        {
            if(d.sr.needs_close())
                // VFALCO Choose an error code
                ec = boost::asio::error::eof;
            break;
        }
        write_some_op<Stream, decltype(d.sr),
            write_op>{std::move(*this), d.s, d.sr}();
        return;
    }
upcall:
    d_.invoke(ec);
}

//------------------------------------------------------------------------------

template<class Stream>
class write_some_lambda
{
    Stream& stream_;

public:
    boost::optional<std::size_t> bytes_transferred;

    explicit
    write_some_lambda(Stream& stream)
        : stream_(stream)
    {
    }

    template<class ConstBufferSequence>
    void
    operator()(error_code& ec,
        ConstBufferSequence const& buffer)
    {
        bytes_transferred =
            stream_.write_some(buffer, ec);
    }
};

} // detail

//------------------------------------------------------------------------------

template<
    class SyncWriteStream,
    bool isRequest, class Body, class Fields,
        class Decorator, class Allocator>
void
write_some(SyncWriteStream& stream, serializer<
    isRequest, Body, Fields, Decorator, Allocator>& sr)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    error_code ec;
    write_some(stream, sr, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<
    class SyncWriteStream,
    bool isRequest, class Body, class Fields,
        class Decorator, class Allocator>
void
write_some(SyncWriteStream& stream, serializer<
    isRequest, Body, Fields, Decorator, Allocator>& sr,
        error_code& ec)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    detail::write_some_lambda<SyncWriteStream> f{stream};
    sr.get(ec, f);
    if(! ec)
    {
        if(f.bytes_transferred)
            sr.consume(*f.bytes_transferred);
        if(sr.is_done() && sr.needs_close())
            // VFALCO decide on an error code
            ec = boost::asio::error::eof;
    }
}

template<class AsyncWriteStream,
    bool isRequest, class Body, class Fields,
        class Decorator, class Allocator, class WriteHandler>
async_return_type<WriteHandler, void(error_code)>
async_write_some(AsyncWriteStream& stream, serializer<
    isRequest, Body, Fields, Decorator, Allocator>& sr,
        WriteHandler&& handler)
{
    static_assert(is_async_write_stream<
            AsyncWriteStream>::value,
        "AsyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    async_completion<WriteHandler,
        void(error_code)> init{handler};
    detail::write_some_op<AsyncWriteStream, decltype(sr),
        handler_type<WriteHandler, void(error_code)>>{
                init.completion_handler, stream, sr}();
    return init.result.get();
}

template<class SyncWriteStream,
    bool isRequest, class Body, class Fields>
void
write(SyncWriteStream& stream,
    message<isRequest, Body, Fields> const& msg)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    error_code ec;
    write(stream, msg, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class SyncWriteStream,
    bool isRequest, class Body, class Fields>
void
write(SyncWriteStream& stream,
    message<isRequest, Body, Fields> const& msg,
        error_code& ec)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    auto sr = make_serializer(msg);
    for(;;)
    {
#if 0
        sr.write_some(stream, ec);
#else
        write_some(stream, sr, ec);
#endif
        if(ec)
            return;
        if(sr.is_done())
            break;
    }
}

template<class AsyncWriteStream,
    bool isRequest, class Body, class Fields,
        class WriteHandler>
async_return_type<
    WriteHandler, void(error_code)>
async_write(AsyncWriteStream& stream,
    message<isRequest, Body, Fields> const& msg,
        WriteHandler&& handler)
{
    static_assert(
        is_async_write_stream<AsyncWriteStream>::value,
        "AsyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    async_completion<WriteHandler,
        void(error_code)> init{handler};
    detail::write_op<AsyncWriteStream, handler_type<
        WriteHandler, void(error_code)>,
            isRequest, Body, Fields>{
                init.completion_handler, stream, msg}(
                    error_code{});
    return init.result.get();
}

//------------------------------------------------------------------------------

template<bool isRequest, class Fields>
std::ostream&
operator<<(std::ostream& os,
    header<isRequest, Fields> const& msg)
{
    detail::write_start_line(os, msg);
    detail::write_fields(os, msg.fields);
    os << "\r\n";
    return os;
}

template<bool isRequest, class Body, class Fields>
std::ostream&
operator<<(std::ostream& os,
    message<isRequest, Body, Fields> const& msg)
{
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    beast::detail::sync_ostream oss{os};
    error_code ec;
    write(oss, msg, ec);
    if(ec && ec != boost::asio::error::eof)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return os;
}

} // http
} // beast

#endif
