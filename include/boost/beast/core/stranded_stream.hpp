//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_STRANDED_STREAM_HPP
#define BOOST_BEAST_CORE_STRANDED_STREAM_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/core/detail/stranded_stream.hpp>
#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/executor.hpp>
#include <boost/core/empty_value.hpp>
#include <type_traits>

namespace boost {
namespace beast {

//------------------------------------------------------------------------------

/** A stream socket using a custom executor, defaulting to a strand

    This class template is parameterized on the executor type to be
    used for all asynchronous operations. This achieves partial support for
    [P1322]. The default template parameter uses a strand for the next
    layer's executor.

    @see <a href="http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1322r0.html">[P1322R0]</a>
         <em>Networking TS enhancement to enable custom I/O executors</em>
*/
template<
    class Protocol,
    class Executor = beast::executor_type<net::basic_stream_socket<Protocol>>
>
class stranded_stream
#ifndef BOOST_BEAST_DOXYGEN
    : private detail::stranded_stream_base<Protocol>
    , private boost::empty_value<Executor>
#endif
{
    // Restricted until P1322R0 is incorporated into Boost.Asio.
    static_assert(
        std::is_convertible<decltype(
            std::declval<Executor const&>().context()),
            net::io_context&>::value,
        "Only net::io_context is currently supported for executor_type::context()");

public:
    /// The type of the executor associated with the object.
    using executor_type = Executor;

    /// The type of the next layer.
    using next_layer_type = net::basic_stream_socket<Protocol>;

    /// The protocol type.
    using protocol_type = Protocol;

    /// The endpoint type.
    using endpoint_type = typename Protocol::endpoint;

    /** Construct the stream without opening it.

        This constructor creates a stream. The underlying socket needs
        to be opened and then connected or accepted before data can be
        sent or received on it.

        @param ctx An object whose type meets the requirements of
        <em>ExecutionContext</em>, which the stream will use to dispatch
        handlers for any asynchronous operations performed on the socket.
        Currently, the only supported type for `ctx` is `net::io_context`.

        @param args A list of parameters forwarded to the constructor of
        the underlying socket.

        @note This function does not participate in overload resolution unless:
        @li `std::is_convertible<ExecutionContext&, net::execution_context&>::value` is `true`, and
        @li `std::is_constructible<executor_type, typename ExecutionContext::executor_type>::value` is `true`.

        @see http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1322r0.html
    */
    template<
        class ExecutionContext,
        class... Args
    #if ! BOOST_BEAST_DOXYGEN
        , class = typename std::enable_if<
        std::is_convertible<
            ExecutionContext&,
            net::execution_context&>::value &&
        std::is_constructible<
            executor_type,
            typename ExecutionContext::executor_type>::value
        >::type
    #endif
    >
    explicit
    stranded_stream(ExecutionContext& ctx, Args&&... args)
        : detail::stranded_stream_base<Protocol>(
            ctx, std::forward<Args>(args)...)
        , boost::empty_value<Executor>(
            boost::empty_init_t{}, ctx.get_executor())
    {
        // Restriction is necessary until Asio fully supports P1322R0
        static_assert(
            std::is_same<ExecutionContext, net::io_context>::value,
            "Only net::io_context is currently supported for ExecutionContext");
    }

    /** Construct the stream without opening it.

        This constructor creates a stream. The underlying socket needs
        to be opened and then connected or accepted before data can be
        sent or received on it.

        @param ex The executor which stream will use to dispatch handlers for
        any asynchronous operations performed on the underlying socket.
        Currently, only executors that return a `net::io_context&` from
        `ex.context()` are supported.

        @param args A list of parameters forwarded to the constructor of
        the underlying socket.

        @see http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1322r0.html
    */
    template<class... Args>
    explicit
    stranded_stream(executor_type const& ex, Args&&... args)
        : detail::stranded_stream_base<Protocol>(
            ex.context(), std::forward<Args>(args)...)
        , boost::empty_value<Executor>(
            boost::empty_init_t{}, ex)
    {
        // Restriction is necessary until Asio fully supports P1322R0
        if(ex.context().get_executor() != this->socket_.get_executor())
            throw std::invalid_argument(
                "ctx.get_executor() != socket.get_executor()");
    }

    /** Move-construct a stream from another stream

        This constructor moves a stream from one object to another.

        The behavior of moving a stream while asynchronous operations
        are outstanding is undefined.

        @param other The other object from which the move will occur. 

        @note Following the move, the moved-from object is in a newly
        constructed state.
    */
    stranded_stream(stranded_stream&& other) = default;

    /** Move-assign a stream from another stream

        This assignment operator moves a stream socket from one object
        to another.

        The behavior of move assignment while asynchronous operations
        are pending is undefined.

        @param other The other basic_timeout_stream object from which the
        move will occur. 

        @note Following the move, the moved-from object is a newly
        constructed state.
    */
    stranded_stream&
    operator=(stranded_stream&& other) = default;

    //--------------------------------------------------------------------------

    /** Return the executor associated with the object.
    
        @return A copy of the executor that stream will use to dispatch handlers.
    */
    executor_type
    get_executor() const noexcept
    {
        return this->get();
    }

    /** Get a reference to the underlying socket.

        This function returns a reference to the next layer
        in a stack of stream layers.

        @return A reference to the next layer in the stack of
        stream layers.
    */
    next_layer_type&
    next_layer() noexcept
    {
        return this->socket_;
    }

    /** Get a reference to the underlying socket.

        This function returns a reference to the next layer in a
        stack of stream layers.

        @return A reference to the next layer in the stack of
        stream layers.
    */
    next_layer_type const&
    next_layer() const noexcept
    {
        return this->socket_;
    }

    /** Start an asynchronous connect.

        This function is used to asynchronously connect a socket to the
        specified remote endpoint. The function call always returns immediately.

        The underlying socket is automatically opened if it is not already open.
        If the connect fails, and the socket was automatically opened, the socket
        is not returned to the closed state.
        
        @param ep The remote endpoint to which the underlying socket will be
        connected. Copies will be made of the endpoint object as required.
        
        @param handler The handler to be called when the operation completes.
        The implementation will take ownership of the handler by move construction.
        The handler must be invocable with this signature:
        @code
        void handler(
            error_code ec         // Result of operation
        );
        @endcode
        Regardless of whether the asynchronous operation completes immediately or
        not, the handler will not be invoked from within this function. Invocation
        of the handler will be performed in a manner equivalent to using
        `net::post()`.
    */
    template<class ConnectHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(ConnectHandler,
        void(error_code))
    async_connect(
        endpoint_type ep,
        ConnectHandler&& handler);

    /** Read some data from the stream.

        This function is used to read data from the stream. The function call will
        block until one or more bytes of data has been read successfully, or until
        an error occurs.
        
        @param buffers The buffers into which the data will be read.
        
        @returns The number of bytes read.
        
        @throws boost::system::system_error Thrown on failure.
        
        @note The `read_some` operation may not read all of the requested number of
        bytes. Consider using the function `net::read` if you need to ensure
        that the requested amount of data is read before the blocking operation
        completes.
    */
    template<class MutableBufferSequence>
    std::size_t
    read_some(MutableBufferSequence const& buffers)
    {
        return this->socket_.read_some(buffers);
    }

    /** Read some data from the stream.

        This function is used to read data from the stream. The function call will
        block until one or more bytes of data has been read successfully, or until
        an error occurs.
        
        @param buffers The buffers into which the data will be read.
        
        @param ec Set to indicate what error occurred, if any.

        @returns The number of bytes read.
                
        @note The `read_some` operation may not read all of the requested number of
        bytes. Consider using the function `net::read` if you need to ensure
        that the requested amount of data is read before the blocking operation
        completes.
    */
    template<class MutableBufferSequence>
    std::size_t
    read_some(
        MutableBufferSequence const& buffers,
        error_code& ec)
    {
        return this->socket_.read_some(buffers, ec);
    }

    /** Start an asynchronous read.

        This function is used to asynchronously read data from the stream.
        The function call always returns immediately.
        
        @param buffers A range of zero or more buffers to read stream data into.
        Although the buffers object may be copied as necessary, ownership of the
        underlying memory blocks is retained by the caller, which must guarantee
        that they remain valid until the handler is called.
        
        @param handler The handler to be called when the operation completes.
        The implementation will take ownership of the handler by move construction.
        The handler must be invocable with this signature:
        @code
        void handler(
            error_code error,               // Result of operation.
            std::size_t bytes_transferred   // Number of bytes read.
        );
        @endcode
        Regardless of whether the asynchronous operation completes immediately or
        not, the handler will not be invoked from within this function. Invocation
        of the handler will be performed in a manner equivalent to using
        `net::post()`.
    */
    template<class MutableBufferSequence, class ReadHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(ReadHandler,
        void(error_code, std::size_t))
    async_read_some(
        MutableBufferSequence const& buffers,
        ReadHandler&& handler)
    {
        BOOST_BEAST_HANDLER_INIT(ReadHandler,
            void(error_code, std::size_t));
        this->socket_.async_read_some(
            buffers,
            detail::bind_default_executor(
                this->get(),
                std::move(init.completion_handler)));
        return init.result.get();
    }

    /** Write some data to the stream.
    
        This function is used to write data on the stream. The function call will
        block until one or more bytes of data has been written successfully, or
        until an error occurs.
        
        @param buffers The data to be written.
        
        @returns The number of bytes written.
        
        @throws boost::system::system_error Thrown on failure.
        
        @note The `write_some` operation may not transmit all of the data to the
        peer. Consider using the function `net::write` if you need to
        ensure that all data is written before the blocking operation completes.
    */
    template<class ConstBufferSequence>
    std::size_t
    write_some(ConstBufferSequence const& buffers)
    {
        return this->socket_.write_some(buffers);
    }

    /** Write some data to the stream.
    
        This function is used to write data on the stream. The function call will
        block until one or more bytes of data has been written successfully, or
        until an error occurs.
        
        @param buffers The data to be written.
        
        @param ec Set to indicate what error occurred, if any.

        @returns The number of bytes written.
                
        @note The `write_some` operation may not transmit all of the data to the
        peer. Consider using the function `net::write` if you need to
        ensure that all data is written before the blocking operation completes.
    */
    template<class ConstBufferSequence>
    std::size_t
    write_some(
        ConstBufferSequence const& buffers,
        error_code& ec)
    {
        return this->socket_.write_some(buffers, ec);
    }

    /** Start an asynchronous write.

        This function is used to asynchronously write data to the stream.
        The function call always returns immediately.
        
        @param buffers A range of zero or more buffers to be written to the stream.
        Although the buffers object may be copied as necessary, ownership of the
        underlying memory blocks is retained by the caller, which must guarantee
        that they remain valid until the handler is called.
        
        @param handler The handler to be called when the operation completes.
        The implementation will take ownership of the handler by move construction.
        The handler must be invocable with this signature:
        @code
        void handler(
            error_code error,               // Result of operation.
            std::size_t bytes_transferred   // Number of bytes written.
        );
        @endcode
        Regardless of whether the asynchronous operation completes immediately or
        not, the handler will not be invoked from within this function. Invocation
        of the handler will be performed in a manner equivalent to using
        `net::post()`.
    */
    template<class ConstBufferSequence, class WriteHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(WriteHandler,
        void(error_code, std::size_t))
    async_write_some(
        ConstBufferSequence const& buffers,
        WriteHandler&& handler)
    {
        BOOST_BEAST_HANDLER_INIT(WriteHandler,
            void(error_code, std::size_t));
        this->socket_.async_write_some(
            buffers,
            detail::bind_default_executor(
                this->get(),
                std::move(init.completion_handler)));
        return init.result.get();
    }
};

} // beast
} // boost

#endif
