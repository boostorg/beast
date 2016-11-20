//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_PARSE_IPP_HPP
#define BEAST_HTTP_IMPL_PARSE_IPP_HPP

#include <beast/http/concepts.hpp>
#include <beast/http/error.hpp>
#include <beast/core/bind_handler.hpp>
#include <beast/core/handler_helpers.hpp>
#include <beast/core/handler_ptr.hpp>
#include <beast/core/stream_concepts.hpp>
#include <boost/assert.hpp>
#include <boost/optional.hpp>

namespace beast {
namespace http {

namespace detail {

template<class Stream,
    class DynamicBuffer, class Parser, class Handler>
class parse_op
{
    struct data
    {
        bool cont;
        Stream& s;
        DynamicBuffer& db;
        Parser& p;
        boost::optional<typename
            DynamicBuffer::mutable_buffers_type> mb;
        int state = 0;

        data(Handler& handler, Stream& s_,
                DynamicBuffer& sb_, Parser& p_)
            : cont(beast_asio_helpers::
                is_continuation(handler))
            , s(s_)
            , db(sb_)
            , p(p_)
        {
            BOOST_ASSERT(p.need_more());
        }
    };

    handler_ptr<data, Handler> d_;

public:
    parse_op(parse_op&&) = default;
    parse_op(parse_op const&) = default;

    template<class DeducedHandler, class... Args>
    parse_op(DeducedHandler&& h, Stream& s, Args&&... args)
        : d_(std::forward<DeducedHandler>(h),
            s, std::forward<Args>(args)...)
    {
        (*this)(error_code{}, 0, false);
    }

    void
    operator()(error_code ec,
        std::size_t bytes_transferred, bool again = true);

    friend
    void* asio_handler_allocate(
        std::size_t size, parse_op* op)
    {
        return beast_asio_helpers::
            allocate(size, op->d_.handler());
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, parse_op* op)
    {
        return beast_asio_helpers::
            deallocate(p, size, op->d_.handler());
    }

    friend
    bool asio_handler_is_continuation(parse_op* op)
    {
        return op->d_->cont;
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, parse_op* op)
    {
        return beast_asio_helpers::
            invoke(f, op->d_.handler());
    }
};

template<class Stream,
    class DynamicBuffer, class Parser, class Handler>
void
parse_op<Stream, DynamicBuffer, Parser, Handler>::
operator()(error_code ec, std::size_t bytes_transferred, bool again)
{
    auto& d = *d_;
    d.cont = d.cont || again;
    while(d.state != 99)
    {
        switch(d.state)
        {
        case 0:
        {
            // Parse any bytes left over in the buffer
            auto const used =
                d.p.write(d.db.data(), ec);
            if(ec)
            {
                // call handler
                d.state = 99;
                d.s.get_io_service().post(
                    bind_handler(std::move(*this), ec, 0));
                return;
            }
            d.db.consume(used);
            if(! d.p.need_more())
            {
                // call handler
                d.state = 99;
                d.s.get_io_service().post(
                    bind_handler(std::move(*this), ec, 0));
                return;
            }
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
                d.state = 99;
                d.s.get_io_service().post(
                    bind_handler(std::move(*this),
                        error::buffer_overflow, 0));
                return;
            }
            // read
            d.state = 2;
            d.s.async_read_some(*d.mb, std::move(*this));
            return;
        }

        case 1:
        {
            // read
            d.state = 2;
            auto const size =
                read_size_helper(d.db, 65536);
            BOOST_ASSERT(size > 0);
            try
            {
                d.mb.emplace(d.db.prepare(size));
            }
            catch(std::length_error const&)
            {
                ec = error::buffer_overflow;
            }
            d.s.async_read_some(*d.mb, std::move(*this));
            return;
        }

        // got data
        case 2:
        {
            if(ec == boost::asio::error::eof)
            {
                ec = {};
                d.p.write_eof(ec);
                if(ec)
                    goto upcall;
                BOOST_ASSERT(! d.p.need_more());
                goto upcall;
            }
            if(ec)
                goto upcall;
            BOOST_ASSERT(bytes_transferred > 0);
            d.db.commit(bytes_transferred);
            auto const used = d.p.write(d.db.data(), ec);
            if(ec)
                goto upcall;
            d.db.consume(used);
            if(! d.p.need_more())
                goto upcall;
            d.state = 1;
            break;
        }
        }
    }
upcall:
    d_.invoke(ec);
}

} // detail

//------------------------------------------------------------------------------

template<class SyncReadStream, class DynamicBuffer, class Parser>
void
parse(SyncReadStream& stream, DynamicBuffer& dynabuf, Parser& parser)
{
    static_assert(is_SyncReadStream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(is_Parser<Parser>::value,
        "Parser requirements not met");
    error_code ec;
    parse(stream, dynabuf, parser, ec);
    if(ec)
        throw system_error{ec};
}

template<class SyncReadStream, class DynamicBuffer, class Parser>
void
parse(SyncReadStream& stream,
    DynamicBuffer& dynabuf, Parser& parser, error_code& ec)
{
    static_assert(is_SyncReadStream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(is_Parser<Parser>::value,
        "Parser requirements not met");
    for(;;)
    {
        auto used =
            parser.write(dynabuf.data(), ec);
        if(ec)
            return;
        dynabuf.consume(used);
        if(! parser.need_more())
            break;
        boost::optional<typename
            DynamicBuffer::mutable_buffers_type> mb;
        auto const size =
            read_size_helper(dynabuf, 65536);
        BOOST_ASSERT(size > 0);
        try
        {
            mb.emplace(dynabuf.prepare(size));
        }
        catch(std::length_error const&)
        {
            ec = error::buffer_overflow;
            return;
        }
        dynabuf.commit(stream.read_some(*mb, ec));
        if(ec == boost::asio::error::eof)
        {
            // Caller will see eof on next read.
            ec = {};
            parser.write_eof(ec);
            if(ec)
                return;
            BOOST_ASSERT(! parser.need_more());
            break;
        }
        if(ec)
            return;
    }
}

template<class AsyncReadStream,
    class DynamicBuffer, class Parser, class ReadHandler>
typename async_completion<
    ReadHandler, void(error_code)>::result_type
async_parse(AsyncReadStream& stream,
    DynamicBuffer& dynabuf, Parser& parser, ReadHandler&& handler)
{
    static_assert(is_AsyncReadStream<AsyncReadStream>::value,
        "AsyncReadStream requirements not met");
    static_assert(is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(is_Parser<Parser>::value,
        "Parser requirements not met");
    beast::async_completion<ReadHandler,
        void(error_code)> completion{handler};
    detail::parse_op<AsyncReadStream, DynamicBuffer,
        Parser, decltype(completion.handler)>{
            completion.handler, stream, dynabuf, parser};
    return completion.result.get();
}

} // http
} // beast

#endif
