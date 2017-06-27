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
#include <beast/core/detail/config.hpp>
#include <boost/asio/handler_alloc_hook.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/asio/write.hpp>
#include <boost/optional.hpp>
#include <boost/throw_exception.hpp>
#include <ostream>
#include <sstream>

namespace beast {
namespace http {
namespace detail {

template<class Stream, bool isRequest,
    class Body, class Fields, class Decorator,
        class Handler>
class write_some_op
{
    Stream& s_;
    serializer<isRequest,
        Body, Fields, Decorator>& sr_;
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
            ConstBufferSequence const& buffer)
        {
            ec.assign(0, ec.category());
            invoked = true;
            return op_.s_.async_write_some(
                buffer, std::move(op_));
        }
    };

public:
    write_some_op(write_some_op&&) = default;
    write_some_op(write_some_op const&) = default;

    template<class DeducedHandler>
    write_some_op(DeducedHandler&& h,
        Stream& s, serializer<isRequest, Body,
            Fields, Decorator>& sr)
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

template<class Stream, bool isRequest,class Body,
    class Fields, class Decorator, class Handler>
void
write_some_op<Stream, isRequest, Body,
    Fields, Decorator, Handler>::
operator()()
{
    error_code ec;
    if(sr_.is_done())
        return s_.get_io_service().post(
            bind_handler(std::move(*this), ec, 0));
    lambda f{*this};
    sr_.get(ec, f);
    if(ec)
    {
        BOOST_ASSERT(! f.invoked);
        return s_.get_io_service().post(
            bind_handler(std::move(*this), ec, 0));
    }
    if(f.invoked)
        // *this has been moved from,
        // cannot access members here.
        return;
    return s_.get_io_service().post(
        bind_handler(std::move(*this), ec, 0));
}

template<class Stream, bool isRequest, class Body,
    class Fields, class Decorator, class Handler>
void
write_some_op<Stream, isRequest, Body,
    Fields, Decorator, Handler>::
operator()(error_code ec, std::size_t bytes_transferred)
{
    if(! ec)
        sr_.consume(bytes_transferred);
    h_(ec);
}

//------------------------------------------------------------------------------

struct serializer_is_header_done
{
    template<bool isRequest, class Body,
        class Fields, class Decorator>
    bool
    operator()(serializer<isRequest, Body,
        Fields, Decorator> const& sr) const
    {
        return sr.is_header_done();
    }
};

struct serializer_is_done
{
    template<bool isRequest, class Body,
        class Fields, class Decorator>
    bool
    operator()(serializer<isRequest, Body,
        Fields, Decorator> const& sr) const
    {
        return sr.is_done();
    }
};

template<class Stream, bool isRequest,
    class Body, class Fields, class Decorator,
        class Predicate, class Handler>
class write_op
{
    int state_ = 0;
    Stream& s_;
    serializer<isRequest,
        Body, Fields, Decorator>& sr_;
    Handler h_;

    class lambda
    {
        write_op& op_;

    public:
        bool invoked = false;

        explicit
        lambda(write_op& op)
            : op_(op)
        {
        }

        template<class ConstBufferSequence>
        void
        operator()(error_code& ec,
            ConstBufferSequence const& buffer)
        {
            ec.assign(0, ec.category());
            invoked = true;
            return op_.s_.async_write_some(
                buffer, std::move(op_));
        }
    };

public:
    write_op(write_op&&) = default;
    write_op(write_op const&) = default;

    template<class DeducedHandler>
    write_op(DeducedHandler&& h, Stream& s,
        serializer<isRequest, Body, Fields,
            Decorator>& sr)
        : s_(s)
        , sr_(sr)
        , h_(std::forward<DeducedHandler>(h))
    {
    }

    void
    operator()(error_code ec,
        std::size_t bytes_transferred);

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

template<class Stream, bool isRequest, class Body,
    class Fields, class Decorator, class Predicate,
        class Handler>
void
write_op<Stream, isRequest, Body, Fields,
    Decorator, Predicate, Handler>::
operator()(error_code ec,
    std::size_t bytes_transferred)
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
        lambda f{*this};
        state_ = 2;
        sr_.get(ec, f);
        if(ec)
        {
            BOOST_ASSERT(! f.invoked);
            return s_.get_io_service().post(
                bind_handler(std::move(*this), ec, 0));
        }
        if(f.invoked)
            // *this has been moved from,
            // cannot access members here.
            return;
        BOOST_ASSERT(Predicate{}(sr_));
        state_ = 1;
        return s_.get_io_service().post(
            bind_handler(std::move(*this), ec, 0));
    }

    case 1:
        goto upcall;

    case 2:
        state_ = 3;
        BEAST_FALLTHROUGH;

    case 3:
    {
        sr_.consume(bytes_transferred);
        if(Predicate{}(sr_))
            goto upcall;
        lambda f{*this};
        sr_.get(ec, f);
        if(ec)
        {
            BOOST_ASSERT(! f.invoked);
            goto upcall;
        }
        if(f.invoked)
            // *this has been moved from,
            // cannot access members here.
            return;
        BOOST_ASSERT(Predicate{}(sr_));
        goto upcall;
    }
    }
upcall:
    h_(ec);
}

//------------------------------------------------------------------------------

template<class Stream, class Handler,
    bool isRequest, class Body, class Fields>
class write_msg_op
{
    struct data
    {
        Stream& s;
        serializer<isRequest,
            Body, Fields, no_chunk_decorator> sr;

        data(Handler&, Stream& s_, message<
                isRequest, Body, Fields>& m_)
            : s(s_)
            , sr(m_, no_chunk_decorator{})
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
    operator()(error_code ec);

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
write_msg_op<Stream, Handler, isRequest, Body, Fields>::
operator()()
{
    auto& d = *d_;
    return async_write(d.s, d.sr, std::move(*this));
}

template<class Stream, class Handler,
    bool isRequest, class Body, class Fields>
void
write_msg_op<Stream, Handler, isRequest, Body, Fields>::
operator()(error_code ec)
{
    auto& d = *d_;
    if(! ec)
        if(d.sr.need_close())
            ec = error::end_of_stream;
    d_.invoke(ec);
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
        ConstBufferSequence const& buffer)
    {
        invoked = true;
        bytes_transferred =
            stream_.write_some(buffer, ec);
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
        ConstBufferSequence const& buffer)
    {
        invoked = true;
        bytes_transferred = boost::asio::write(
            stream_, buffer, ec);
    }
};

} // detail

//------------------------------------------------------------------------------

template<class SyncWriteStream, bool isRequest,
    class Body, class Fields, class Decorator>
void
write_some(SyncWriteStream& stream, serializer<
    isRequest, Body, Fields, Decorator>& sr)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    error_code ec;
    write_some(stream, sr, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class SyncWriteStream,
    bool isRequest, class Body, class Fields,
        class Decorator>
void
write_some(SyncWriteStream& stream, serializer<
    isRequest, Body, Fields, Decorator>& sr,
        error_code& ec)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    detail::write_some_lambda<SyncWriteStream> f{stream};
    if(sr.is_done())
    {
        ec.assign(0, ec.category());
        return;
    }
    sr.get(ec, f);
    if(ec)
        return;
    if(f.invoked)
        sr.consume(f.bytes_transferred);
}

template<class AsyncWriteStream,
    bool isRequest, class Body, class Fields,
        class Decorator, class WriteHandler>
async_return_type<WriteHandler, void(error_code)>
async_write_some(AsyncWriteStream& stream, serializer<
    isRequest, Body, Fields, Decorator>& sr,
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
    detail::write_some_op<AsyncWriteStream, isRequest,
        Body, Fields, Decorator, handler_type<
            WriteHandler, void(error_code)>>{
                init.completion_handler, stream, sr}();
    return init.result.get();
}

//------------------------------------------------------------------------------

template<class SyncWriteStream,
    bool isRequest, class Body, class Fields,
        class Decorator>
void
write_header(SyncWriteStream& stream, serializer<
    isRequest, Body, Fields, Decorator>& sr)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    error_code ec;
    write_header(stream, sr, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class SyncWriteStream,
    bool isRequest, class Body, class Fields,
        class Decorator>
void
write_header(SyncWriteStream& stream, serializer<
    isRequest, Body, Fields, Decorator>& sr,
        error_code& ec)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    sr.split(true);
    if(sr.is_header_done())
    {
        ec.assign(0, ec.category());
        return;
    }
    detail::write_lambda<SyncWriteStream> f{stream};
    do
    {
        sr.get(ec, f);
        if(ec)
            return;
        BOOST_ASSERT(f.invoked);
        sr.consume(f.bytes_transferred);
    }
    while(! sr.is_header_done());
}

template<class AsyncWriteStream,
    bool isRequest, class Body, class Fields,
        class Decorator, class WriteHandler>
async_return_type<WriteHandler, void(error_code)>
async_write_header(AsyncWriteStream& stream, serializer<
    isRequest, Body, Fields, Decorator>& sr,
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
    async_completion<WriteHandler,
        void(error_code)> init{handler};
    detail::write_op<AsyncWriteStream, isRequest, Body, Fields,
        Decorator, detail::serializer_is_header_done,
            handler_type<WriteHandler, void(error_code)>>{
                init.completion_handler, stream, sr}(
                    error_code{}, 0);
    return init.result.get();
}

//------------------------------------------------------------------------------

template<class SyncWriteStream,
    bool isRequest, class Body, class Fields,
        class Decorator>
void
write(SyncWriteStream& stream, serializer<
    isRequest, Body, Fields, Decorator>& sr)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    error_code ec;
    write(stream, sr, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class SyncWriteStream,
    bool isRequest, class Body, class Fields,
        class Decorator>
void
write(SyncWriteStream& stream, serializer<
    isRequest, Body, Fields, Decorator>& sr,
        error_code& ec)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    sr.split(false);
    if(sr.is_done())
    {
        ec.assign(0, ec.category());
        return;
    }
    detail::write_lambda<SyncWriteStream> f{stream};
    do
    {
        sr.get(ec, f);
        if(ec)
            return;
        if(f.invoked)
            sr.consume(f.bytes_transferred);
    }
    while(! sr.is_done());
    if(sr.need_close())
        ec = error::end_of_stream;
}

template<class AsyncWriteStream,
    bool isRequest, class Body, class Fields,
        class Decorator, class WriteHandler>
async_return_type<WriteHandler, void(error_code)>
async_write(AsyncWriteStream& stream, serializer<
    isRequest, Body, Fields, Decorator>& sr,
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
    async_completion<WriteHandler,
        void(error_code)> init{handler};
    detail::write_op<AsyncWriteStream, isRequest, Body,
        Fields, Decorator, detail::serializer_is_done,
            handler_type<WriteHandler, void(error_code)>>{
                init.completion_handler, stream, sr}(
                    error_code{}, 0);
    return init.result.get();
}

//------------------------------------------------------------------------------

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
    serializer<isRequest, Body, Fields> sr{msg};
    write(stream, sr, ec);
}

template<class AsyncWriteStream,
    bool isRequest, class Body, class Fields,
        class WriteHandler>
async_return_type<
    WriteHandler, void(error_code)>
async_write(AsyncWriteStream& stream,
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
    async_completion<WriteHandler,
        void(error_code)> init{handler};
    detail::write_msg_op<AsyncWriteStream, handler_type<
        WriteHandler, void(error_code)>,
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
        h, h.version, h.method()};
    return os << buffers(fr.get());
}

template<class Fields>
std::ostream&
operator<<(std::ostream& os,
    header<false, Fields> const& h)
{
    typename Fields::reader fr{
        h, h.version, h.result_int()};
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
        sr.get(ec, f);
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

#endif
