//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_IMPL_WRITE_IPP
#define BOOST_BEAST_HTTP_IMPL_WRITE_IPP

#include <boost/beast/http/type_traits.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/ostream.hpp>
#include <boost/beast/core/handler_ptr.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/handler_alloc_hook.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/asio/write.hpp>
#include <boost/optional.hpp>
#include <boost/throw_exception.hpp>
#include <ostream>
#include <sstream>

namespace boost {
namespace beast {
namespace http {
namespace detail {

template<
    class Stream, class Handler,
    bool isRequest, class Body, class Fields>
class write_some_op
{
    Stream& s_;
    serializer<isRequest,Body, Fields>& sr_;
    Handler h_;

    class lambda
    {
        write_some_op& op_;

    public:
        bool invoked = false;

        explicit
        lambda(write_some_op& op)
            : op_(op)
        {
        }

        template<class ConstBufferSequence>
        void
        operator()(error_code& ec,
            ConstBufferSequence const& buffers)
        {
            invoked = true;
            ec.assign(0, ec.category());
            return op_.s_.async_write_some(
                buffers, std::move(op_));
        }
    };

public:
    write_some_op(write_some_op&&) = default;
    write_some_op(write_some_op const&) = default;

    template<class DeducedHandler>
    write_some_op(DeducedHandler&& h, Stream& s,
            serializer<isRequest, Body, Fields>& sr)
        : s_(s)
        , sr_(sr)
        , h_(std::forward<DeducedHandler>(h))
    {
    }

    void
    operator()();

    void
    operator()(
        error_code ec,
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
    class Stream, class Handler,
    bool isRequest, class Body, class Fields>
void
write_some_op<
    Stream, Handler, isRequest, Body, Fields>::
operator()()
{
    error_code ec;
    if(! sr_.is_done())
    {
        lambda f{*this};
        sr_.next(ec, f);
        if(ec)
        {
            BOOST_ASSERT(! f.invoked);
            return s_.get_io_service().post(
                bind_handler(std::move(*this), ec, 0));
        }
        if(f.invoked)
        {
            // *this has been moved from,
            // cannot access members here.
            return;
        }
        // What else could it be?
        BOOST_ASSERT(sr_.is_done());
    }
    return s_.get_io_service().post(
        bind_handler(std::move(*this), ec, 0));
}

template<
    class Stream, class Handler,
    bool isRequest, class Body, class Fields>
void
write_some_op<
    Stream, Handler, isRequest, Body, Fields>::
operator()(
    error_code ec, std::size_t bytes_transferred)
{
    if(! ec)
    {
        sr_.consume(bytes_transferred);
        if(sr_.is_done())
            if(! sr_.keep_alive())
                ec = error::end_of_stream;
    }
    h_(ec, bytes_transferred);
}

//------------------------------------------------------------------------------

struct serializer_is_header_done
{
    template<
        bool isRequest, class Body, class Fields>
    bool
    operator()(
        serializer<isRequest, Body, Fields>& sr) const
    {
        return sr.is_header_done();
    }
};

struct serializer_is_done
{
    template<
        bool isRequest, class Body, class Fields>
    bool
    operator()(
        serializer<isRequest, Body, Fields>& sr) const
    {
        return sr.is_done();
    }
};

//------------------------------------------------------------------------------

template<
    class Stream, class Handler, class Predicate,
    bool isRequest, class Body, class Fields>
class write_op
{
    int state_ = 0;
    Stream& s_;
    serializer<isRequest, Body, Fields>& sr_;
    std::size_t bytes_transferred_ = 0;
    Handler h_;

public:
    write_op(write_op&&) = default;
    write_op(write_op const&) = default;

    template<class DeducedHandler>
    write_op(DeducedHandler&& h, Stream& s,
            serializer<isRequest, Body, Fields>& sr)
        : s_(s)
        , sr_(sr)
        , h_(std::forward<DeducedHandler>(h))
    {
    }

    void
    operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0);

    friend
    void* asio_handler_allocate(
        std::size_t size, write_op* op)
    {
        using boost::asio::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(op->h_));
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, write_op* op)
    {
        using boost::asio::asio_handler_deallocate;
        asio_handler_deallocate(
            p, size, std::addressof(op->h_));
    }

    friend
    bool asio_handler_is_continuation(write_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return op->state_ >= 3 ||
            asio_handler_is_continuation(
                std::addressof(op->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, write_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(
            f, std::addressof(op->h_));
    }
};

template<
    class Stream, class Handler, class Predicate,
    bool isRequest, class Body, class Fields>
void
write_op<Stream, Handler, Predicate,
    isRequest, Body, Fields>::
operator()(
    error_code ec, std::size_t bytes_transferred)
{
    if(ec)
        goto upcall;
    switch(state_)
    {
    case 0:
    {
        if(Predicate{}(sr_))
        {
            state_ = 1;
            return s_.get_io_service().post(
                bind_handler(std::move(*this), ec, 0));
        }
        state_ = 2;
        return beast::http::async_write_some(
            s_, sr_, std::move(*this));
    }

    case 1:
        goto upcall;

    case 2:
        state_ = 3;
        BOOST_BEAST_FALLTHROUGH;

    case 3:
    {
        bytes_transferred_ += bytes_transferred;
        if(Predicate{}(sr_))
            goto upcall;
        return beast::http::async_write_some(
            s_, sr_, std::move(*this));
    }
    }
upcall:
    h_(ec, bytes_transferred_);
}

//------------------------------------------------------------------------------

template<class Stream, class Handler,
    bool isRequest, class Body, class Fields>
class write_msg_op
{
    struct data
    {
        Stream& s;
        serializer<isRequest, Body, Fields> sr;

        data(Handler&, Stream& s_, message<
                isRequest, Body, Fields>& m_)
            : s(s_)
            , sr(m_)
        {
        }
    };

    handler_ptr<data, Handler> d_;

public:
    write_msg_op(write_msg_op&&) = default;
    write_msg_op(write_msg_op const&) = default;

    template<class DeducedHandler, class... Args>
    write_msg_op(DeducedHandler&& h, Stream& s, Args&&... args)
        : d_(std::forward<DeducedHandler>(h),
            s, std::forward<Args>(args)...)
    {
    }

    void
    operator()();

    void
    operator()(
        error_code ec, std::size_t bytes_transferred);

    friend
    void* asio_handler_allocate(
        std::size_t size, write_msg_op* op)
    {
        using boost::asio::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(op->d_.handler()));
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, write_msg_op* op)
    {
        using boost::asio::asio_handler_deallocate;
        asio_handler_deallocate(
            p, size, std::addressof(op->d_.handler()));
    }

    friend
    bool asio_handler_is_continuation(write_msg_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->d_.handler()));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, write_msg_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(
            f, std::addressof(op->d_.handler()));
    }
};

template<class Stream, class Handler,
    bool isRequest, class Body, class Fields>
void
write_msg_op<
    Stream, Handler, isRequest, Body, Fields>::
operator()()
{
    auto& d = *d_;
    return async_write(d.s, d.sr, std::move(*this));
}

template<class Stream, class Handler,
    bool isRequest, class Body, class Fields>
void
write_msg_op<
    Stream, Handler, isRequest, Body, Fields>::
operator()(error_code ec, std::size_t bytes_transferred)
{
    d_.invoke(ec, bytes_transferred);
}

//------------------------------------------------------------------------------

template<class Stream>
class write_some_lambda
{
    Stream& stream_;

public:
    bool invoked = false;
    std::size_t bytes_transferred = 0;

    explicit
    write_some_lambda(Stream& stream)
        : stream_(stream)
    {
    }

    template<class ConstBufferSequence>
    void
    operator()(error_code& ec,
        ConstBufferSequence const& buffers)
    {
        invoked = true;
        bytes_transferred =
            stream_.write_some(buffers, ec);
    }
};

template<class Stream>
class write_lambda
{
    Stream& stream_;

public:
    bool invoked = false;
    std::size_t bytes_transferred = 0;

    explicit
    write_lambda(Stream& stream)
        : stream_(stream)
    {
    }

    template<class ConstBufferSequence>
    void
    operator()(error_code& ec,
        ConstBufferSequence const& buffers)
    {
        invoked = true;
        bytes_transferred = boost::asio::write(
            stream_, buffers, ec);
    }
};

template<
    class SyncWriteStream,
    bool isRequest, class Body, class Fields>
std::size_t
write_some(
    SyncWriteStream& stream,
    serializer<isRequest, Body, Fields>& sr,
    error_code& ec)
{
    if(! sr.is_done())
    {
        write_some_lambda<SyncWriteStream> f{stream};
        sr.next(ec, f);
        if(ec)
            return f.bytes_transferred;
        if(f.invoked)
            sr.consume(f.bytes_transferred);
        if(sr.is_done())
            if(! sr.keep_alive())
                ec = error::end_of_stream;
        return f.bytes_transferred;
    }
    if(! sr.keep_alive())
        ec = error::end_of_stream;
    else
        ec.assign(0, ec.category());
    return 0;
}

template<
    class AsyncWriteStream,
    bool isRequest, class Body, class Fields,
    class WriteHandler>
async_return_type<
    WriteHandler,
    void(error_code, std::size_t)>
async_write_some(
    AsyncWriteStream& stream,
    serializer<isRequest, Body, Fields>& sr,
    WriteHandler&& handler)
{
    async_completion<
        WriteHandler,
        void(error_code, std::size_t)> init{handler};
    detail::write_some_op<
        AsyncWriteStream,
        handler_type<WriteHandler,
            void(error_code, std::size_t)>,
        isRequest, Body, Fields>{
            init.completion_handler, stream, sr}();
    return init.result.get();
}

} // detail

//------------------------------------------------------------------------------

template<
    class SyncWriteStream,
    bool isRequest, class Body, class Fields>
std::size_t
write_some(
    SyncWriteStream& stream,
    serializer<isRequest, Body, Fields>& sr)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    error_code ec;
    auto const bytes_transferred =
        write_some(stream, sr, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<
    class SyncWriteStream,
    bool isRequest, class Body, class Fields>
std::size_t
write_some(
    SyncWriteStream& stream,
    serializer<isRequest, Body, Fields>& sr,
    error_code& ec)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    return detail::write_some(stream, sr, ec);
}

template<
    class AsyncWriteStream,
    bool isRequest, class Body, class Fields,
    class WriteHandler>
async_return_type<
    WriteHandler,
    void(error_code, std::size_t)>
async_write_some(
    AsyncWriteStream& stream,
    serializer<isRequest, Body, Fields>& sr,
    WriteHandler&& handler)
{
    static_assert(is_async_write_stream<
            AsyncWriteStream>::value,
        "AsyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    return detail::async_write_some(stream, sr,
        std::forward<WriteHandler>(handler));
}

//------------------------------------------------------------------------------

template<
    class SyncWriteStream,
    bool isRequest, class Body, class Fields>
std::size_t
write_header(SyncWriteStream& stream,
    serializer<isRequest, Body, Fields>& sr)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    error_code ec;
    auto const bytes_transferred =
        write_header(stream, sr, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<
    class SyncWriteStream,
    bool isRequest, class Body, class Fields>
std::size_t
write_header(
    SyncWriteStream& stream,
    serializer<isRequest, Body, Fields>& sr,
    error_code& ec)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    sr.split(true);
    std::size_t bytes_transferred = 0;
    if(! sr.is_header_done())
    {
        detail::write_lambda<SyncWriteStream> f{stream};
        do
        {
            sr.next(ec, f);
            bytes_transferred += f.bytes_transferred;
            if(ec)
                return bytes_transferred;
            BOOST_ASSERT(f.invoked);
            sr.consume(f.bytes_transferred);
        }
        while(! sr.is_header_done());
    }
    else
    {
        ec.assign(0, ec.category());
    }
    return bytes_transferred;
}

template<
    class AsyncWriteStream,
    bool isRequest, class Body, class Fields,
    class WriteHandler>
async_return_type<
    WriteHandler,
    void(error_code, std::size_t)>
async_write_header(
    AsyncWriteStream& stream,
    serializer<isRequest, Body, Fields>& sr,
    WriteHandler&& handler)
{
    static_assert(is_async_write_stream<
            AsyncWriteStream>::value,
        "AsyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    sr.split(true);
    async_completion<
        WriteHandler,
        void(error_code, std::size_t)> init{handler};
    detail::write_op<
        AsyncWriteStream,
        handler_type<WriteHandler,
            void(error_code, std::size_t)>,
        detail::serializer_is_header_done,
        isRequest, Body, Fields>{
        init.completion_handler, stream, sr}();
    return init.result.get();
}

//------------------------------------------------------------------------------

template<
    class SyncWriteStream,
    bool isRequest, class Body, class Fields>
std::size_t
write(
    SyncWriteStream& stream,
    serializer<isRequest, Body, Fields>& sr)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    error_code ec;
    auto const bytes_transferred =
        write(stream, sr, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<
    class SyncWriteStream,
    bool isRequest, class Body, class Fields>
std::size_t
write(
    SyncWriteStream& stream,
    serializer<isRequest, Body, Fields>& sr,
    error_code& ec)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    std::size_t bytes_transferred = 0;
    sr.split(false);
    for(;;)
    {
        bytes_transferred +=
            write_some(stream, sr, ec);
        if(ec)
            return bytes_transferred;
        if(sr.is_done())
            break;
    }
    return bytes_transferred;
}

template<
    class AsyncWriteStream,
    bool isRequest, class Body, class Fields,
    class WriteHandler>
async_return_type<
    WriteHandler,
    void(error_code, std::size_t)>
async_write(
    AsyncWriteStream& stream,
    serializer<isRequest, Body, Fields>& sr,
    WriteHandler&& handler)
{
    static_assert(is_async_write_stream<
            AsyncWriteStream>::value,
        "AsyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    sr.split(false);
    async_completion<
        WriteHandler,
        void(error_code, std::size_t)> init{handler};
    detail::write_op<
        AsyncWriteStream,
        handler_type<WriteHandler,
            void(error_code, std::size_t)>,
        detail::serializer_is_done,
        isRequest, Body, Fields>{
            init.completion_handler, stream, sr}();
    return init.result.get();
}

//------------------------------------------------------------------------------

template<
    class SyncWriteStream,
    bool isRequest, class Body, class Fields>
std::size_t
write(
    SyncWriteStream& stream,
    message<isRequest, Body, Fields> const& msg)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    error_code ec;
    auto const bytes_transferred =
        write(stream, msg, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<
    class SyncWriteStream,
    bool isRequest, class Body, class Fields>
std::size_t
write(
    SyncWriteStream& stream,
    message<isRequest, Body, Fields> const& msg,
    error_code& ec)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    serializer<isRequest, Body, Fields> sr{msg};
    return write(stream, sr, ec);
}

template<
    class AsyncWriteStream,
    bool isRequest, class Body, class Fields,
    class WriteHandler>
async_return_type<
    WriteHandler,
    void(error_code, std::size_t)>
async_write(
    AsyncWriteStream& stream,
    message<isRequest, Body, Fields>& msg,
    WriteHandler&& handler)
{
    static_assert(
        is_async_write_stream<AsyncWriteStream>::value,
        "AsyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    async_completion<
        WriteHandler,
        void(error_code, std::size_t)> init{handler};
    detail::write_msg_op<
        AsyncWriteStream,
        handler_type<WriteHandler,
            void(error_code, std::size_t)>,
        isRequest, Body, Fields>{
            init.completion_handler, stream, msg}();
    return init.result.get();
}

//------------------------------------------------------------------------------

namespace detail {

template<class Serializer>
class write_ostream_lambda
{
    std::ostream& os_;
    Serializer& sr_;

public:
    write_ostream_lambda(std::ostream& os,
            Serializer& sr)
        : os_(os)
        , sr_(sr)
    {
    }

    template<class ConstBufferSequence>
    void
    operator()(error_code& ec,
        ConstBufferSequence const& buffers) const
    {
        ec.assign(0, ec.category());
        if(os_.fail())
            return;
        std::size_t bytes_transferred = 0;
        using boost::asio::buffer_cast;
        using boost::asio::buffer_size;
        for(auto it = buffers.begin();
            it != buffers.end(); ++it)
        {
            boost::asio::const_buffer b = *it;
            auto const n = buffer_size(b);
            os_.write(buffer_cast<char const*>(b), n);
            if(os_.fail())
                return;
            bytes_transferred += n;
        }
        sr_.consume(bytes_transferred);
    }
};

} // detail

template<class Fields>
std::ostream&
operator<<(std::ostream& os,
    header<true, Fields> const& h)
{
    typename Fields::reader fr{
        h, h.version(), h.method()};
    return os << buffers(fr.get());
}

template<class Fields>
std::ostream&
operator<<(std::ostream& os,
    header<false, Fields> const& h)
{
    typename Fields::reader fr{
        h, h.version(), h.result_int()};
    return os << buffers(fr.get());
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
    serializer<isRequest, Body, Fields> sr{msg};
    error_code ec;
    detail::write_ostream_lambda<decltype(sr)> f{os, sr};
    do
    {
        sr.next(ec, f);
        if(os.fail())
            break;
        if(ec == error::end_of_stream)
            ec.assign(0, ec.category());
        if(ec)
        {
            os.setstate(std::ios::failbit);
            break;
        }   
    }
    while(! sr.is_done());
    return os;
}

} // http
} // beast
} // boost

#endif
