//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <beast/core.hpp>
#include <boost/asio.hpp>
#include <cstddef>
#include <memory>
#include <utility>

//[core_sample_echo_op_1

/** Asynchronously read a line and echo it back.

    This function is used to asynchronously read a line ending
    in a carriage-return linefeed ("CRLF") from the stream,
    and then write it back. The function call always returns
    immediately. The asynchronous operation will continue until
    one of the following conditions is true:

    @li A line was read in and sent back on the stream

    @li An error occurs.

    This operation is implemented in terms of one or more calls to
    the stream's `async_read_some` and `async_write_some` functions,
    and is known as a <em>composed operation</em>. The program must
    ensure that the stream performs no other operations until this
    operation completes. The implementation may read additional octets
    that lie past the end of the line being read. These octets are
    silently discarded.

    @param The stream to operate on. The type must meet the
    requirements of @b AsyncReadStream and @AsyncWriteStream

    @param token The completion token to use. If this is a
    completion handler, copies will be made as required.
    The signature of the handler must be:
    @code
    void handler(
        error_code& ec      // result of operation
    );
    @endcode
    Regardless of whether the asynchronous operation completes
    immediately or not, the handler will not be invoked from within
    this function. Invocation of the handler will be performed in a
    manner equivalent to using `boost::asio::io_service::post`.
*/
template<
    class AsyncStream,
    class CompletionToken>
beast::async_return_type<       /*< The [link beast.ref.async_return_type `async_return_type`] customizes the return value based on the completion token >*/
    CompletionToken,
    void(beast::error_code)>    /*< This is the signature for the completion handler >*/
async_echo(
    AsyncStream& stream,
    CompletionToken&& token);

//]

//[core_sample_echo_op_3

// This composed operation reads a line of input and echoes it back.
//
template<class AsyncStream, class Handler>
class echo_op
{
    // This holds all of the state information required by the operation.
    struct state
    {
        // The stream to read and write to
        AsyncStream& stream;

        // Indicates what step in the operation's state machine
        // to perform next, starting from zero.
        int step = 0;

        // The buffer used to hold the input and output data.
        // Note that we use a custom allocator for performance,
        // this allows the implementation of the io_service to
        // make efficient re-use of memory allocated by composed
        // operations during continuations.
        //
        boost::asio::basic_streambuf<beast::handler_alloc<char, Handler>> buffer;

        // handler_ptr requires that the first parameter to the
        // contained object constructor is a reference to the
        // managed final completion handler.
        //
        explicit state(Handler& handler, AsyncStream& stream_)
            : stream(stream_)
            , buffer((std::numeric_limits<std::size_t>::max)(),
                beast::handler_alloc<char, Handler>{handler})
        {
        }
    };

    // This smart pointer container allocates our state using the
    // memory allocation hooks associated with the final completion
    // handler, manages the lifetime of that handler for us, and
    // enforces the destroy-before-invocation requirement on memory
    // allocated using the hooks.
    //
    beast::handler_ptr<state, Handler> p_;

public:
    // Boost.Asio requires that handlers are CopyConstructible.
    // In some cases, it takes advantage of handlers that are
    // MoveConstructible. This operation supports both.
    //
    echo_op(echo_op&&) = default;
    echo_op(echo_op const&) = default;

    // The constructor simply creates our state variables in
    // the smart pointer container.
    //
    template<class DeducedHandler, class... Args>
    echo_op(AsyncStream& stream, DeducedHandler&& handler)
        : p_(std::forward<DeducedHandler>(handler), stream)
    {
    }

    // Determines if the next asynchronous operation represents a
    // continuation of the asynchronous flow of control associated
    // with the final handler. If we are past step one, it means
    // we have performed an asynchronous operation therefore any
    // subsequent operation would represent a continuation.
    // Otherwise, we propagate the handler's associated value of
    // is_continuation. Getting this right means the implementation
    // may schedule the invokation of the invoked functions more
    // efficiently.
    //
    friend bool asio_handler_is_continuation(echo_op* op)
    {
        // This next call is structured to permit argument
        // dependent lookup to take effect.
        using boost::asio::asio_handler_is_continuation;

        // Always use std::addressof to pass the pointer to the handler,
        // otherwise an unwanted overload of operator& may be called instead.
        return op->p_->step > 1 ||
            asio_handler_is_continuation(std::addressof(op->p_.handler()));
    }

    // Handler hook forwarding. These free functions invoke the hooks
    // associated with the final completion handler. In effect, they
    // make the Asio implementation treat our composed operation the
    // same way it would treat the final completion handler for the
    // purpose of memory allocation and invocation.
    //
    // Our implementation just passes through the call to the hook
    // associated with the final handler.

    friend void* asio_handler_allocate(std::size_t size, echo_op* op)
    {
        using boost::asio::asio_handler_allocate;
        return asio_handler_allocate(size, std::addressof(op->p_.handler()));
    }

    friend void asio_handler_deallocate(void* p, std::size_t size, echo_op* op)
    {
        using boost::asio::asio_handler_deallocate;
        return asio_handler_deallocate(p, size, std::addressof(op->p_.handler()));
    }

    template<class Function>
    friend void asio_handler_invoke(Function&& f, echo_op* op)
    {
        using boost::asio::asio_handler_invoke;
        return asio_handler_invoke(f, std::addressof(op->p_.handler()));
    }

    // Our main entry point. This will get called as our
    // intermediate operations complete. Definition below.
    //
    void operator()(beast::error_code ec, std::size_t bytes_transferred);
};

//]

//[core_sample_echo_op_4

// echo_op is callable with the signature void(error_code, bytes_transferred),
// allowing `*this` to be used as both a ReadHandler and a WriteHandler.
//
template<class AsyncStream, class Handler>
void echo_op<AsyncStream, Handler>::
operator()(beast::error_code ec, std::size_t bytes_transferred)
{
    // Store a reference to our state. The address of the state won't
    // change, and this solves the problem where dereferencing the
    // data member is undefined after a move.
    auto& p = *p_;

    // Now perform the next step in the state machine
    switch(ec ? 2 : p.step)
    {
        // initial entry
        case 0:
            // read up to the first newline
            p.step = 1;
            return boost::asio::async_read_until(p.stream, p.buffer, "\n", std::move(*this));

        case 1:
            // write everything back
            p.step = 2;
            // async_read_until could have read past the newline,
            // use buffer_prefix to make sure we only send one line
            return boost::asio::async_write(p.stream,
                beast::buffer_prefix(bytes_transferred, p.buffer.data()), std::move(*this));

        case 2:
            p.buffer.consume(bytes_transferred);
            break;
    }

    // Invoke the final handler. If we wanted to pass any arguments
    // which come from our state, they would have to be moved to the
    // stack first, since the `handler_ptr` guarantees that the state
    // is destroyed before the handler is invoked.
    //
    p_.invoke(ec);
    return;
}

//]

//[core_sample_echo_op_2

template<class AsyncStream, class Handler>
class echo_op;

// Read a line and echo it back
//
template<class AsyncStream, class CompletionToken>
beast::async_return_type<CompletionToken, void(beast::error_code)>
async_echo(AsyncStream& stream, CompletionToken&& token)
{
    // Make sure stream meets the requirements. We use static_assert
    // to cause a friendly message instead of an error novel.
    //
    static_assert(beast::is_async_stream<AsyncStream>::value,
        "AsyncStream requirements not met");

    // This helper manages some of the handler's lifetime and
    // uses the result and handler specializations associated with
    // the completion token to help customize the return value.
    //
    beast::async_completion<CompletionToken, void(beast::error_code)> init{token};

    // Create the composed operation and launch it. This is a constructor
    // call followed by invocation of operator(). We use handler_type
    // to convert the completion token into the correct handler type,
    // allowing user defined specializations of the async result template
    // to take effect.
    //
    echo_op<AsyncStream, beast::handler_type<CompletionToken, void(beast::error_code)>>{
        stream, init.completion_handler}(beast::error_code{}, 0);

    // This hook lets the caller see a return value when appropriate.
    // For example this might return std::future<error_code> if
    // CompletionToken is boost::asio::use_future, or this might
    // return an error code if CompletionToken specifies a coroutine.
    //
    return init.result.get();
}

//]

int main()
{
    using address_type = boost::asio::ip::address;
    using socket_type = boost::asio::ip::tcp::socket;
    using endpoint_type = boost::asio::ip::tcp::endpoint;

    // Create a listening socket, accept a connection, perform
    // the echo, and then shut everything down and exit.
    boost::asio::io_service ios;
    socket_type sock{ios};
    boost::asio::ip::tcp::acceptor acceptor{ios};
    endpoint_type ep{address_type::from_string("0.0.0.0"), 0};
    acceptor.open(ep.protocol());
    acceptor.bind(ep);
    acceptor.listen();
    acceptor.accept(sock);
    async_echo(sock,
        [&](beast::error_code ec)
        {
        });
    ios.run();
    return 0;
}
