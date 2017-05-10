//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_ASYNC_READ_IPP_HPP
#define BEAST_HTTP_IMPL_ASYNC_READ_IPP_HPP

#include <beast/http/concepts.hpp>
#include <beast/http/error.hpp>
#include <beast/http/message_parser.hpp>
#include <beast/http/read.hpp>
#include <beast/core/bind_handler.hpp>
#include <beast/core/handler_helpers.hpp>
#include <beast/core/handler_ptr.hpp>
#include <beast/core/stream_concepts.hpp>
#include <boost/assert.hpp>
#include <boost/optional.hpp>

namespace beast {
namespace http {
namespace detail {

template<class Stream, class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived,
        class Handler>
class read_some_buffer_op
{
    struct data
    {
        bool cont;
        Stream& s;
        DynamicBuffer& db;
        basic_parser<isRequest, isDirect, Derived>& p;
        boost::optional<typename
            DynamicBuffer::mutable_buffers_type> mb;
        boost::optional<typename
            Derived::mutable_buffers_type> bb;
        std::size_t bytes_used;
        int state = 0;

        data(Handler& handler, Stream& s_, DynamicBuffer& db_,
                basic_parser<isRequest, isDirect, Derived>& p_)
            : cont(beast_asio_helpers::
                is_continuation(handler))
            , s(s_)
            , db(db_)
            , p(p_)
        {
        }
    };

    handler_ptr<data, Handler> d_;

public:
    read_some_buffer_op(read_some_buffer_op&&) = default;
    read_some_buffer_op(read_some_buffer_op const&) = default;

    template<class DeducedHandler, class... Args>
    read_some_buffer_op(DeducedHandler&& h,
            Stream& s, Args&&... args)
        : d_(std::forward<DeducedHandler>(h),
            s, std::forward<Args>(args)...)
    {
        (*this)(error_code{}, 0, false);
    }

    void
    operator()(error_code ec,
        std::size_t bytes_transferred, bool again = true);

    friend
    void*
    asio_handler_allocate(std::size_t size,
        read_some_buffer_op* op)
    {
        return beast_asio_helpers::
            allocate(size, op->d_.handler());
    }

    friend
    void
    asio_handler_deallocate(
        void* p, std::size_t size,
            read_some_buffer_op* op)
    {
        return beast_asio_helpers::
            deallocate(p, size, op->d_.handler());
    }

    friend
    bool
    asio_handler_is_continuation(
        read_some_buffer_op* op)
    {
        return op->d_->cont;
    }

    template<class Function>
    friend
    void
    asio_handler_invoke(Function&& f,
        read_some_buffer_op* op)
    {
        return beast_asio_helpers::
            invoke(f, op->d_.handler());
    }
};

template<class Stream, class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived,
        class Handler>
void
read_some_buffer_op<Stream, DynamicBuffer,
    isRequest, isDirect, Derived, Handler>::
operator()(error_code ec,
    std::size_t bytes_transferred, bool again)
{
    auto& d = *d_;
    d.cont = d.cont || again;
    if(d.state == 99)
        goto upcall;
    for(;;)
    {
        switch(d.state)
        {
        case 0:
            if(d.db.size() == 0)
            {
                d.state = 2;
                break;
            }
            //[[fallthrough]]

        case 1:
        {
            BOOST_ASSERT(d.db.size() > 0);
            d.bytes_used =
                d.p.write(d.db.data(), ec);
            if(d.bytes_used > 0 || ec)
            {
                // call handler
                if(d.state == 1)
                    goto upcall;
                d.state = 99;
                d.s.get_io_service().post(
                    bind_handler(std::move(*this), ec, 0));
                return;
            }
            //[[fallthrough]]
        }

        case 2:
        case 3:
        {
            auto const size =
                read_size_helper(d.db, 65536);
            BOOST_ASSERT(size > 0);
            try
            {
                d.mb.emplace(d.db.prepare(size));
            }
            catch(std::length_error const&)
            {
                // call handler
                if(d.state == 3)
                    goto upcall;
                d.state = 99;
                d.s.get_io_service().post(
                    bind_handler(std::move(*this),
                        error::buffer_overflow, 0));
                return;
            }
            // read
            d.state = 4;
            d.s.async_read_some(*d.mb, std::move(*this));
            return;
        }

        case 4:
            if(ec == boost::asio::error::eof)
            {
                BOOST_ASSERT(bytes_transferred == 0);
                d.bytes_used = 0;
                if(! d.p.got_some())
                {
                    ec = error::end_of_stream;
                    goto upcall;
                }
                // caller sees end_of_stream on next read.
                ec = {};
                d.p.write_eof(ec);
                if(ec)
                    goto upcall;
                BOOST_ASSERT(d.p.is_complete());
                goto upcall;
            }
            if(ec)
            {
                d.bytes_used = 0;
                goto upcall;
            }
            BOOST_ASSERT(bytes_transferred > 0);
            d.db.commit(bytes_transferred);
            d.state = 1;
            break;
        }
    }
upcall:
    // can't pass any members of `d` otherwise UB
    auto const bytes_used = d.bytes_used;
    d_.invoke(ec, bytes_used);
}

//------------------------------------------------------------------------------

template<class Stream, class DynamicBuffer,
    bool isRequest, class Derived, class Handler>
class read_some_body_op
{
    struct data
    {
        bool cont;
        Stream& s;
        DynamicBuffer& db;
        basic_parser<isRequest, true, Derived>& p;
        boost::optional<typename
            Derived::mutable_buffers_type> mb;
        std::size_t bytes_used;
        int state = 0;

        data(Handler& handler, Stream& s_, DynamicBuffer& db_,
                basic_parser<isRequest, true, Derived>& p_)
            : cont(beast_asio_helpers::
                is_continuation(handler))
            , s(s_)
            , db(db_)
            , p(p_)
        {
        }
    };

    handler_ptr<data, Handler> d_;

public:
    read_some_body_op(read_some_body_op&&) = default;
    read_some_body_op(read_some_body_op const&) = default;

    template<class DeducedHandler, class... Args>
    read_some_body_op(DeducedHandler&& h,
            Stream& s, Args&&... args)
        : d_(std::forward<DeducedHandler>(h),
            s, std::forward<Args>(args)...)
    {
        (*this)(error_code{}, 0, false);
    }

    void
    operator()(error_code ec,
        std::size_t bytes_transferred, bool again = true);

    friend
    void*
    asio_handler_allocate(std::size_t size,
        read_some_body_op* op)
    {
        return beast_asio_helpers::
            allocate(size, op->d_.handler());
    }

    friend
    void
    asio_handler_deallocate(
        void* p, std::size_t size,
            read_some_body_op* op)
    {
        return beast_asio_helpers::
            deallocate(p, size, op->d_.handler());
    }

    friend
    bool
    asio_handler_is_continuation(
        read_some_body_op* op)
    {
        return op->d_->cont;
    }

    template<class Function>
    friend
    void
    asio_handler_invoke(Function&& f,
        read_some_body_op* op)
    {
        return beast_asio_helpers::
            invoke(f, op->d_.handler());
    }
};

template<class Stream, class DynamicBuffer,
    bool isRequest, class Derived, class Handler>
void
read_some_body_op<Stream, DynamicBuffer,
    isRequest, Derived, Handler>::
operator()(error_code ec,
    std::size_t bytes_transferred, bool again)
{
    auto& d = *d_;
    d.cont = d.cont || again;
    if(d.state == 99)
        goto upcall;
    for(;;)
    {
        switch(d.state)
        {
        case 0:
            if(d.db.size() > 0)
            {
                d.bytes_used = d.p.copy_body(d.db);
                // call handler
                d.state = 99;
                d.s.get_io_service().post(
                    bind_handler(std::move(*this),
                        ec, 0));
                return;
            }
            d.p.prepare_body(d.mb, 65536);
            // read
            d.state = 1;
            d.s.async_read_some(
                *d.mb, std::move(*this));
            return;

        case 1:
            d.bytes_used = 0;
            if(ec == boost::asio::error::eof)
            {
                BOOST_ASSERT(bytes_transferred == 0);
                // caller sees EOF on next read
                ec = {};
                d.p.write_eof(ec);
                if(ec)
                    goto upcall;
                BOOST_ASSERT(d.p.is_complete());
            }
            else if(! ec)
            {
                d.p.commit_body(bytes_transferred);
            }
            goto upcall;
        }
    }
upcall:
    // can't pass any members of `d` otherwise UB
    auto const bytes_used = d.bytes_used;
    d_.invoke(ec, bytes_used);
}

//------------------------------------------------------------------------------

template<class Stream, class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived,
        class Handler>
class parse_op
{
    struct data
    {
        bool cont;
        Stream& s;
        DynamicBuffer& db;
        basic_parser<isRequest, isDirect, Derived>& p;

        data(Handler& handler, Stream& s_, DynamicBuffer& db_,
            basic_parser<isRequest, isDirect, Derived>& p_)
            : cont(beast_asio_helpers::
                is_continuation(handler))
            , s(s_)
            , db(db_)
            , p(p_)
        {
        }
    };

    handler_ptr<data, Handler> d_;

public:
    parse_op(parse_op&&) = default;
    parse_op(parse_op const&) = default;

    template<class DeducedHandler, class... Args>
    parse_op(DeducedHandler&& h,
            Stream& s, Args&&... args)
        : d_(std::forward<DeducedHandler>(h),
            s, std::forward<Args>(args)...)
    {
        (*this)(error_code{}, 0, false);
    }

    void
    operator()(error_code const& ec,
        std::size_t bytes_used, bool again = true);

    friend
    void*
    asio_handler_allocate(
        std::size_t size, parse_op* op)
    {
        return beast_asio_helpers::
            allocate(size, op->d_.handler());
    }

    friend
    void
    asio_handler_deallocate(
        void* p, std::size_t size,
            parse_op* op)
    {
        return beast_asio_helpers::
            deallocate(p, size, op->d_.handler());
    }

    friend
    bool
    asio_handler_is_continuation(
        parse_op* op)
    {
        return op->d_->cont;
    }

    template<class Function>
    friend
    void
    asio_handler_invoke(
        Function&& f, parse_op* op)
    {
        return beast_asio_helpers::
            invoke(f, op->d_.handler());
    }
};

template<class Stream, class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived,
        class Handler>
void
parse_op<Stream, DynamicBuffer,
    isRequest, isDirect, Derived, Handler>::
operator()(error_code const& ec,
    std::size_t bytes_used, bool again)
{
    auto& d = *d_;
    d.cont = d.cont || again;
    if(! ec)
    {
        d.db.consume(bytes_used);
        if(! d.p.is_complete())
            return async_read_some(
                d.s, d.db, d.p, std::move(*this));
    }
    d_.invoke(ec);
}

//------------------------------------------------------------------------------

template<class Stream, class DynamicBuffer,
    bool isRequest, class Body, class Fields,
        class Handler>
class read_message_op
{
    using parser_type =
        message_parser<isRequest, Body, Fields>;

    using message_type =
        message<isRequest, Body, Fields>;

    struct data
    {
        bool cont;
        Stream& s;
        DynamicBuffer& db;
        message_type& m;
        parser_type p;
        bool started = false;
        int state = 0;

        data(Handler& handler, Stream& s_,
                DynamicBuffer& sb_, message_type& m_)
            : cont(beast_asio_helpers::
                is_continuation(handler))
            , s(s_)
            , db(sb_)
            , m(m_)
        {
        }
    };

    handler_ptr<data, Handler> d_;

public:
    read_message_op(read_message_op&&) = default;
    read_message_op(read_message_op const&) = default;

    template<class DeducedHandler, class... Args>
    read_message_op(DeducedHandler&& h, Stream& s, Args&&... args)
        : d_(std::forward<DeducedHandler>(h),
            s, std::forward<Args>(args)...)
    {
        (*this)(error_code{}, false);
    }

    void
    operator()(error_code ec, bool again = true);

    friend
    void* asio_handler_allocate(
        std::size_t size, read_message_op* op)
    {
        return beast_asio_helpers::
            allocate(size, op->d_.handler());
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, read_message_op* op)
    {
        return beast_asio_helpers::
            deallocate(p, size, op->d_.handler());
    }

    friend
    bool asio_handler_is_continuation(read_message_op* op)
    {
        return op->d_->cont;
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, read_message_op* op)
    {
        return beast_asio_helpers::
            invoke(f, op->d_.handler());
    }
};

template<class Stream, class DynamicBuffer,
    bool isRequest, class Body, class Fields,
        class Handler>
void
read_message_op<Stream, DynamicBuffer, isRequest, Body, Fields, Handler>::
operator()(error_code ec, bool again)
{
    auto& d = *d_;
    d.cont = d.cont || again;
    if(ec)
        goto upcall;
    switch(d.state)
    {
        case 0:
            d.state = 1;
            beast::http::async_read(
                d.s, d.db, d.p, std::move(*this));
            return;

        case 1:
            d.m = d.p.release();
            goto upcall;
    }
upcall:
    d_.invoke(ec);
}

template<
    class AsyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived,
    class ReadHandler>
BEAST_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
async_read_some(
    AsyncReadStream& stream,
    DynamicBuffer& dynabuf,
    basic_parser<isRequest, true, Derived>& parser,
    ReadHandler&& handler)
{
    async_completion<ReadHandler,
        void(error_code, std::size_t)> init{handler};
    switch(parser.state())
    {
    case parse_state::header:
    case parse_state::chunk_header:
        detail::read_some_buffer_op<AsyncReadStream,
            DynamicBuffer, isRequest, true, Derived, BEAST_HANDLER_TYPE(
                ReadHandler, void(error_code, std::size_t))>{
                    init.completion_handler, stream, dynabuf, parser};
        break;

    default:
        detail::read_some_body_op<AsyncReadStream,
            DynamicBuffer, isRequest, Derived, BEAST_HANDLER_TYPE(
                ReadHandler, void(error_code, std::size_t))>{
                    init.completion_handler, stream, dynabuf, parser};
        break;
    }
    return init.result.get();
}

template<
    class AsyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived,
    class ReadHandler>
inline
BEAST_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
async_read_some(
    AsyncReadStream& stream,
    DynamicBuffer& dynabuf,
    basic_parser<isRequest, false, Derived>& parser,
    ReadHandler&& handler)
{
    async_completion<ReadHandler,
        void(error_code, std::size_t)> init{handler};
    detail::read_some_buffer_op<AsyncReadStream,
        DynamicBuffer, isRequest, false, Derived, BEAST_HANDLER_TYPE(
            ReadHandler, void(error_code, std::size_t))>{
                init.completion_handler, stream, dynabuf, parser};
    return init.result.get();
}

} // detail

//------------------------------------------------------------------------------

template<
    class AsyncReadStream,
    class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived,
    class ReadHandler>
BEAST_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
async_read_some(
    AsyncReadStream& stream,
    DynamicBuffer& dynabuf,
    basic_parser<isRequest, isDirect, Derived>& parser,
    ReadHandler&& handler)
{
    static_assert(is_AsyncReadStream<AsyncReadStream>::value,
        "AsyncReadStream requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_ASSERT(! parser.is_complete());
    return detail::async_read_some(stream, dynabuf, parser,
        std::forward<ReadHandler>(handler));
}

template<
    class AsyncReadStream,
    class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived,
    class ReadHandler>
BEAST_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code))
async_read(
    AsyncReadStream& stream,
    DynamicBuffer& dynabuf,
    basic_parser<isRequest, isDirect, Derived>& parser,
    ReadHandler&& handler)
{
    static_assert(is_AsyncReadStream<AsyncReadStream>::value,
        "AsyncReadStream requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_ASSERT(! parser.is_complete());
    async_completion<ReadHandler,
        void(error_code)> init{handler};
    detail::parse_op<AsyncReadStream, DynamicBuffer,
        isRequest, isDirect, Derived, BEAST_HANDLER_TYPE(
            ReadHandler, void(error_code))>{
                init.completion_handler, stream, dynabuf, parser};
    return init.result.get();
}

template<
    class AsyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Body, class Fields,
    class ReadHandler>
BEAST_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code))
async_read(
    AsyncReadStream& stream,
    DynamicBuffer& dynabuf,
    message<isRequest, Body, Fields>& msg,
    ReadHandler&& handler)
{
    static_assert(is_AsyncReadStream<AsyncReadStream>::value,
        "AsyncReadStream requirements not met");
    static_assert(is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(is_Body<Body>::value,
        "Body requirements not met");
    static_assert(has_reader<Body>::value,
        "Body has no reader");
    static_assert(is_Reader<typename Body::reader,
        message<isRequest, Body, Fields>>::value,
            "Reader requirements not met");
    async_completion<ReadHandler,
        void(error_code)> init{handler};
    detail::read_message_op<AsyncReadStream, DynamicBuffer,
        isRequest, Body, Fields, BEAST_HANDLER_TYPE(
            ReadHandler, void(error_code))>{
                init.completion_handler, stream, dynabuf, msg};
    return init.result.get();
}

} // http
} // beast

#endif
