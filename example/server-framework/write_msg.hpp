//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_EXAMPLE_SERVER_WRITE_MSG_HPP
#define BEAST_EXAMPLE_SERVER_WRITE_MSG_HPP

#include "server.hpp"

#include <beast/core/async_result.hpp>
#include <beast/core/handler_ptr.hpp>
#include <beast/core/type_traits.hpp>
#include <beast/http/message.hpp>
#include <beast/http/write.hpp>
#include <beast/http/type_traits.hpp>
#include <boost/asio/handler_alloc_hook.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>

namespace framework {

namespace detail {

/** Composed operation to send an HTTP message

    This implements the composed operation needed for the
    @ref async_write_msg function.
*/
template<
    class AsyncWriteStream,
    class Handler,
    bool isRequest, class Body, class Fields>
class write_msg_op
{
    struct data
    {
        AsyncWriteStream& stream;
        beast::http::message<isRequest, Body, Fields> msg;

        data(
            Handler& handler,
            AsyncWriteStream& stream_,
            beast::http::message<isRequest, Body, Fields>&& msg_)
            : stream(stream_)
            , msg(std::move(msg_))
        {
        }
    };

    beast::handler_ptr<data, Handler> d_;

public:
    write_msg_op(write_msg_op&&) = default;
    write_msg_op(write_msg_op const&) = default;

    template<class DeducedHandler, class... Args>
    write_msg_op(DeducedHandler&& h, AsyncWriteStream& s, Args&&... args)
        : d_(std::forward<DeducedHandler>(h),
            s, std::forward<Args>(args)...)
    {
    }

    void
    operator()()
    {
        auto& d = *d_;
        beast::http::async_write(
            d.stream, d.msg, std::move(*this));
    }

    void
    operator()(error_code ec)
    {
        d_.invoke(ec);
    }

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
        return asio_handler_is_continuation(std::addressof(op->d_.handler()));
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

} // detail

/** Write an HTTP message to a stream asynchronously

    This function is used to write a complete message to a stream asynchronously
    using HTTP/1. The function call always returns immediately. The asynchronous
    operation will continue until one of the following conditions is true:

    @li The entire message is written.

    @li An error occurs.

    This operation is implemented in terms of zero or more calls to the stream's
    `async_write_some` function, and is known as a <em>composed operation</em>.
    The program must ensure that the stream performs no other write operations
    until this operation completes. The algorithm will use a temporary
    @ref serializer with an empty chunk decorator to produce buffers. If
    the semantics of the message indicate that the connection should be
    closed after the message is sent, the error delivered by this function
    will be @ref error::end_of_stream

    @param stream The stream to which the data is to be written.
    The type must support the @b AsyncWriteStream concept.

    @param msg The message to write. The function will take ownership
    of the object as if by move constrction.

    @param handler The handler to be called when the operation
    completes. Copies will be made of the handler as required.
    The equivalent function signature of the handler must be:
    @code void handler(
        error_code const& error // result of operation
    ); @endcode
    Regardless of whether the asynchronous operation completes
    immediately or not, the handler will not be invoked from within
    this function. Invocation of the handler will be performed in a
    manner equivalent to using `boost::asio::io_service::post`.
*/
template<
    class AsyncWriteStream,
    bool isRequest, class Body, class Fields,
    class WriteHandler>
beast::async_return_type<WriteHandler, void(error_code)>
async_write_msg(
    AsyncWriteStream& stream,
    beast::http::message<isRequest, Body, Fields>&& msg,
    WriteHandler&& handler)
{
    static_assert(
        beast::is_async_write_stream<AsyncWriteStream>::value,
        "AsyncWriteStream requirements not met");

    static_assert(beast::http::is_body<Body>::value,
        "Body requirements not met");

    static_assert(beast::http::is_body_reader<Body>::value,
        "BodyReader requirements not met");

    beast::async_completion<WriteHandler, void(error_code)> init{handler};

    detail::write_msg_op<
        AsyncWriteStream,
        beast::handler_type<WriteHandler, void(error_code)>,
        isRequest, Body, Fields>{
            init.completion_handler,
            stream,
            std::move(msg)}();

    return init.result.get();
}

} // framework

#endif
