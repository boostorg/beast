//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_STRANDED_SOCKET_HPP
#define BOOST_BEAST_CORE_STRANDED_SOCKET_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/core/detail/stranded_socket.hpp>
#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/core/empty_value.hpp>
#include <type_traits>

namespace boost {
namespace beast {

//------------------------------------------------------------------------------

/** A stream-oriented socket using a custom executor, defaulting to a strand

    This class template provides asynchronous and blocking stream-oriented
    socket functionality. It is designed as a replacement for
    `net::basic_stream_socket`.

    This class template is parameterized on the executor type to be
    used for all asynchronous operations. This achieves partial support for
    [P1322]. The default template parameter uses a strand for the next
    layer's executor.

    Unlike other stream wrappers, the underlying socket is accessed
    through the @ref socket member function instead of `next_layer`.
    This causes @ref stranded_socket to be returned in calls to
    @ref get_lowest_layer.

    @tparam Protocol The protocol to use.
 
    @tparam Executor The executor to use.

    @par Thread Safety
    @e Distinct @e objects: Safe.@n
    @e Shared @e objects: Unsafe.
 
    @par Concepts:
    @li SyncReadStream, SyncWriteStream
    @li AsyncReadStream, AsyncWriteStream,
    @li Protocol
    @li Executor

    @see <a href="http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1322r0.html">[P1322R0]</a>
         <em>Networking TS enhancement to enable custom I/O executors</em>
*/
template<
    class Protocol,
    class Executor = net::io_context::strand
>
class stranded_socket
#ifndef BOOST_BEAST_DOXYGEN
    : private detail::stranded_socket_base<Protocol>
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

    /// The type of the underlying socket.
    using socket_type = net::basic_stream_socket<Protocol>;

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
    stranded_socket(ExecutionContext& ctx, Args&&... args)
        : detail::stranded_socket_base<Protocol>(
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
    stranded_socket(executor_type const& ex, Args&&... args)
        : detail::stranded_socket_base<Protocol>(
            ex.context(), std::forward<Args>(args)...)
        , boost::empty_value<Executor>(
            boost::empty_init_t{}, ex)
    {
    }

    /** Move-construct a stream from another stream

        This constructor moves a stream from one object to another.

        The behavior of moving a stream while asynchronous operations
        are outstanding is undefined.

        @param other The other object from which the move will occur. 

        @note Following the move, the moved-from object is in a newly
        constructed state.
    */
    stranded_socket(stranded_socket&& other) = default;

    /// Move assignment (deleted)
    stranded_socket&
    operator=(stranded_socket&& other) = delete;

    /** Get a reference to the underlying socket.

        This function returns a reference to the underlying
        socket used by the stream.
    */
    socket_type&
    socket() noexcept
    {
        return this->socket_;
    }

    /** Get a reference to the underlying socket.

        This function returns a reference to the underlying
        socket used by the stream.
    */
    socket_type const&
    socket() const noexcept
    {
        return this->socket_;
    }

    //--------------------------------------------------------------------------

    /** Return the executor associated with the object.
    
        @return A copy of the executor that stream will use to dispatch handlers.
    */
    executor_type
    get_executor() const noexcept
    {
        return this->get();
    }

    /** Connect the socket to the specified endpoint.

        This function is used to connect a socket to the specified remote endpoint.
        The function call will block until the connection is successfully made or
        an error occurs.

        The socket is automatically opened if it is not already open. If the
        connect fails, and the socket was automatically opened, the socket is
        not returned to the closed state.

        @param ep The remote endpoint to which the socket will be
        connected.

        @throws system_error Thrown on failure.
    */
    void
    connect(endpoint_type const& ep)
    {
        this->socket_.connect(ep);
    }

    /** Connect the socket to the specified endpoint.
        
        This function is used to connect a socket to the specified remote endpoint.
        The function call will block until the connection is successfully made or
        an error occurs.
        
        The socket is automatically opened if it is not already open. If the
        connect fails, and the socket was automatically opened, the socket is
        not returned to the closed state.
        
        @param ep The remote endpoint to which the socket will be
        connected.
        
        @param ec Set to indicate what error occurred, if any.
    */
    void
    connect(endpoint_type const& ep, error_code& ec)
    {
        this->socket_.connect(ep, ec);
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
        endpoint_type const& ep,
        ConnectHandler&& handler)
    {
        BOOST_BEAST_HANDLER_INIT(ConnectHandler,
            void(error_code));
        this->socket_.async_connect(
            ep,
            detail::bind_default_executor(
                this->get(),
                std::move(init.completion_handler)));
        return init.result.get();
    }

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

//------------------------------------------------------------------------------

/** Establishes a socket connection by trying each endpoint in a sequence.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the socket's @c connect member
    function, once for each endpoint in the sequence, until a connection is
    successfully established.
    
    @param socket The socket to be connected.
    If the underlying socket is already open, it will be closed.
    
    @param endpoints A sequence of endpoints.
    
    @returns The successfully connected endpoint.
    
    @throws system_error Thrown on failure. If the sequence is
    empty, the associated error code is `net::error::not_found`.
    Otherwise, contains the error from the last connection attempt.
*/
template<
    class Protocol, class Executor,
    class EndpointSequence
#if ! BOOST_BEAST_DOXYGEN
    ,class = typename std::enable_if<
        net::is_endpoint_sequence<
            EndpointSequence>::value>::type
#endif
>
typename Protocol::endpoint
connect(
    stranded_socket<Protocol, Executor>& socket,
    EndpointSequence const & endpoints
)
{
    return net::connect(socket.socket(), endpoints);
}

/** Establishes a socket connection by trying each endpoint in a sequence.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the socket's @c connect member
    function, once for each endpoint in the sequence, until a connection is
    successfully established.
    
    @param socket The socket to be connected.
    If the underlying socket is already open, it will be closed.
    
    @param endpoints A sequence of endpoints.
    
    @param ec Set to indicate what error occurred, if any. If the sequence is
    empty, set to `net::error::not_found`. Otherwise, contains the error
    from the last connection attempt.
    
    @returns On success, the successfully connected endpoint. Otherwise, a
    default-constructed endpoint.
*/    
template<
    class Protocol, class Executor,
    class EndpointSequence
#if ! BOOST_BEAST_DOXYGEN
    ,class = typename std::enable_if<
        net::is_endpoint_sequence<
            EndpointSequence>::value>::type
#endif
>
typename Protocol::endpoint
connect(
    stranded_socket<Protocol>& socket,
    EndpointSequence const& endpoints,
    error_code& ec
)
{
    return net::connect(socket.socket(), endpoints, ec);
}

/** Establishes a socket connection by trying each endpoint in a sequence.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the socket's @c connect member
    function, once for each endpoint in the sequence, until a connection is
    successfully established.
    
    @param socket The socket to be connected.
    If the underlying socket is already open, it will be closed.
    
    @param begin An iterator pointing to the start of a sequence of endpoints.
    
    @param end An iterator pointing to the end of a sequence of endpoints.
    
    @returns An iterator denoting the successfully connected endpoint.
    
    @throws system_error Thrown on failure. If the sequence is
    empty, the associated error code is `net::error::not_found`.
    Otherwise, contains the error from the last connection attempt.
*/    
template<
    class Protocol, class Executor,
    class Iterator>
Iterator
connect(
    stranded_socket<Protocol, Executor>& socket,
    Iterator begin, Iterator end)
{
    return net::connect(socket.socket(), begin, end);
}

/** Establishes a socket connection by trying each endpoint in a sequence.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the socket's @c connect member
    function, once for each endpoint in the sequence, until a connection is
    successfully established.
    
    @param socket The socket to be connected.
    If the underlying socket is already open, it will be closed.
    
    @param begin An iterator pointing to the start of a sequence of endpoints.
    
    @param end An iterator pointing to the end of a sequence of endpoints.
    
    @param ec Set to indicate what error occurred, if any. If the sequence is
    empty, set to boost::asio::error::not_found. Otherwise, contains the error
    from the last connection attempt.
    
    @returns On success, an iterator denoting the successfully connected
    endpoint. Otherwise, the end iterator.
*/
template<
    class Protocol, class Executor,
    class Iterator>
Iterator
connect(
    stranded_socket<Protocol, Executor>& socket,
    Iterator begin, Iterator end,
    error_code& ec)
{
    return net::connect(socket.socket(), begin, end, ec);
}

/** Establishes a socket connection by trying each endpoint in a sequence.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the socket's @c connect member
    function, once for each endpoint in the sequence, until a connection is
    successfully established.
    
    @param socket The socket to be connected.
    If the underlying socket is already open, it will be closed.
    
    @param endpoints A sequence of endpoints.
    
    @param connect_condition A function object that is called prior to each
    connection attempt. The signature of the function object must be:
    @code
    bool connect_condition(
        error_code const& ec,
        typename Protocol::endpoint const& next);
    @endcode
    The @c ec parameter contains the result from the most recent connect
    operation. Before the first connection attempt, @c ec is always set to
    indicate success. The @c next parameter is the next endpoint to be tried.
    The function object should return true if the next endpoint should be tried,
    and false if it should be skipped.
    
    @returns The successfully connected endpoint.
    
    @throws boost::system::system_error Thrown on failure. If the sequence is
    empty, the associated error code is `net::error::not_found`.
    Otherwise, contains the error from the last connection attempt.
*/
template<
    class Protocol, class Executor,
    class EndpointSequence, class ConnectCondition
#if ! BOOST_BEAST_DOXYGEN
    ,class = typename std::enable_if<
        net::is_endpoint_sequence<
            EndpointSequence>::value>::type
#endif
>
typename Protocol::endpoint
connect(
    stranded_socket<Protocol, Executor>& socket,
    EndpointSequence const& endpoints,
    ConnectCondition connect_condition
)
{
    return net::connect(socket.socket(), endpoints, connect_condition);
}

/** Establishes a socket connection by trying each endpoint in a sequence.

    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the socket's @c connect member
    function, once for each endpoint in the sequence, until a connection is
    successfully established.

    @param socket The socket to be connected.
    If the underlying socket is already open, it will be closed.

    @param endpoints A sequence of endpoints.

    @param connect_condition A function object that is called prior to each
    connection attempt. The signature of the function object must be:
    @code
    bool connect_condition(
        error_code const& ec,
        typename Protocol::endpoint const& next);
    @endcode
    The @c ec parameter contains the result from the most recent connect
    operation. Before the first connection attempt, @c ec is always set to
    indicate success. The @c next parameter is the next endpoint to be tried.
    The function object should return true if the next endpoint should be tried,
    and false if it should be skipped.

    @param ec Set to indicate what error occurred, if any. If the sequence is
    empty, set to `net::error::not_found`. Otherwise, contains the error
    from the last connection attempt.

    @returns On success, the successfully connected endpoint. Otherwise, a
    default-constructed endpoint.
*/
template<
    class Protocol, class Executor,
    class EndpointSequence, class ConnectCondition
#if ! BOOST_BEAST_DOXYGEN
    ,class = typename std::enable_if<
        net::is_endpoint_sequence<
            EndpointSequence>::value>::type
#endif
>
typename Protocol::endpoint
connect(
    stranded_socket<Protocol, Executor>& socket,
    EndpointSequence const& endpoints,
    ConnectCondition connect_condition,
    error_code& ec)
{
    return net::connect(socket.socket(), endpoints, connect_condition, ec);
}

/** Establishes a socket connection by trying each endpoint in a sequence.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the socket's @c connect member
    function, once for each endpoint in the sequence, until a connection is
    successfully established.
    
    @param socket The socket to be connected.
    If the underlying socket is already open, it will be closed.
    
    @param begin An iterator pointing to the start of a sequence of endpoints.
    
    @param end An iterator pointing to the end of a sequence of endpoints.
    
    @param connect_condition A function object that is called prior to each
    connection attempt. The signature of the function object must be:
    @code
    bool connect_condition(
        error_code const& ec,
        typename Protocol::endpoint const& next);
    @endcode
    The @c ec parameter contains the result from the most recent connect
    operation. Before the first connection attempt, @c ec is always set to
    indicate success. The @c next parameter is the next endpoint to be tried.
    The function object should return true if the next endpoint should be tried,
    and false if it should be skipped.
    
    @returns An iterator denoting the successfully connected endpoint.
    
    @throws boost::system::system_error Thrown on failure. If the sequence is
    empty, the associated @c error_code is `net::error::not_found`.
    Otherwise, contains the error from the last connection attempt.
*/  
template<
    class Protocol, class Executor,
    class Iterator, class ConnectCondition>
Iterator
connect(
    stranded_socket<Protocol, Executor>& socket,
    Iterator begin, Iterator end,
    ConnectCondition connect_condition)
{
    return net::connect(socket.socket(), begin, end, connect_condition);
}

/** Establishes a socket connection by trying each endpoint in a sequence.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the socket's @c connect member
    function, once for each endpoint in the sequence, until a connection is
    successfully established.
    
    @param socket The socket to be connected.
    If the underlying socket is already open, it will be closed.
    
    @param begin An iterator pointing to the start of a sequence of endpoints.
    
    @param end An iterator pointing to the end of a sequence of endpoints.
    
    @param connect_condition A function object that is called prior to each
    connection attempt. The signature of the function object must be:
    @code
    bool connect_condition(
        error_code const& ec,
        typename Protocol::endpoint const& next);
    @endcode
    The @c ec parameter contains the result from the most recent connect
    operation. Before the first connection attempt, @c ec is always set to
    indicate success. The @c next parameter is the next endpoint to be tried.
    The function object should return true if the next endpoint should be tried,
    and false if it should be skipped.
    
    @param ec Set to indicate what error occurred, if any. If the sequence is
    empty, set to `net::error::not_found`. Otherwise, contains the error
    from the last connection attempt.
    
    @returns On success, an iterator denoting the successfully connected
    endpoint. Otherwise, the end iterator.
*/
template<
    class Protocol, class Executor,
    class Iterator, class ConnectCondition>
Iterator
connect(
    stranded_socket<Protocol, Executor>& socket,
    Iterator begin, Iterator end,
    ConnectCondition connect_condition,
    error_code& ec)
{
    return net::connect(socket.socket(), begin, end, connect_condition, ec);
}

/** Asynchronously establishes a socket connection by trying each endpoint in a sequence, and terminating if a timeout occurs.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the underlying socket's
    @c async_connect member function, once for each endpoint in the sequence,
    until a connection is successfully established or a timeout occurs.
    
    @param socket The socket to be connected.
    If the underlying socket is already open, it will be closed.
    
    @param endpoints A sequence of endpoints. This this object must meet
    the requirements of <em>EndpointSequence</em>.
    
    @param handler The handler to be called when the connect operation
    completes. Ownership of the handler may be transferred. The function
    signature of the handler must be:
    @code
    void handler(
        // Result of operation. if the sequence is empty, set to
        // net::error::not_found. Otherwise, contains the
        // error from the last connection attempt.
        error_code const& error,
    
        // On success, the successfully connected endpoint.
        // Otherwise, a default-constructed endpoint.
        typename Protocol::endpoint const& endpoint
    );
    @endcode
    Regardless of whether the asynchronous operation completes immediately or
    not, the handler will not be invoked from within this function. Invocation
    of the handler will be performed in a manner equivalent to using
    `net::io_context::post()`.
*/
template<
    class Protocol, class Executor,
    class EndpointSequence,
    class RangeConnectHandler
#if ! BOOST_BEAST_DOXYGEN
    ,class = typename std::enable_if<
        net::is_endpoint_sequence<
            EndpointSequence>::value>::type
#endif
>
BOOST_ASIO_INITFN_RESULT_TYPE(RangeConnectHandler,
    void (error_code, typename Protocol::endpoint))
async_connect(
    stranded_socket<Protocol, Executor>& socket,
    EndpointSequence const& endpoints,
    RangeConnectHandler&& handler)
{
    BOOST_BEAST_HANDLER_INIT(RangeConnectHandler,
        void(error_code, typename Protocol::endpoint));
    net::async_connect(socket.socket(),
        endpoints,
        detail::bind_default_executor(
            socket.get_executor(),
            std::move(init.completion_handler)));
    return init.result.get();
}

/** Asynchronously establishes a socket connection by trying each endpoint in a sequence, and terminating if a timeout occurs.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the underlying socket's
    @c async_connect member function, once for each endpoint in the sequence,
    until a connection is successfully established or a timeout occurs.
    
    @param socket The socket to be connected.
    If the underlying socket is already open, it will be closed.

    @param endpoints A sequence of endpoints. This this object must meet
    the requirements of <em>EndpointSequence</em>.
    
    @param connect_condition A function object that is called prior to each
    connection attempt. The signature of the function object must be:
    @code
    bool connect_condition(
        error_code const& ec,
        typename Protocol::endpoint const& next);
    @endcode
    The @c ec parameter contains the result from the most recent connect
    operation. Before the first connection attempt, @c ec is always set to
    indicate success. The @c next parameter is the next endpoint to be tried.
    The function object should return true if the next endpoint should be tried,
    and false if it should be skipped.

    @param handler The handler to be called when the connect operation
    completes. Ownership of the handler may be transferred. The function
    signature of the handler must be:
    @code
    void handler(
        // Result of operation. if the sequence is empty, set to
        // net::error::not_found. Otherwise, contains the
        // error from the last connection attempt.
        error_code const& error,
    
        // On success, the successfully connected endpoint.
        // Otherwise, a default-constructed endpoint.
        typename Protocol::endpoint const& endpoint
    );
    @endcode
    Regardless of whether the asynchronous operation completes immediately or
    not, the handler will not be invoked from within this function. Invocation
    of the handler will be performed in a manner equivalent to using
    `net::io_context::post()`.
    
    @par Example
    The following connect condition function object can be used to output
    information about the individual connection attempts:
    @code
    struct my_connect_condition
    {
        bool operator()(
            error_code const& ec,
            net::ip::tcp::endpoint const& next)
        {
            if (ec)
                std::cout << "Error: " << ec.message() << std::endl;
            std::cout << "Trying: " << next << std::endl;
            return true;
        }
    };
    @endcode
*/
template<
    class Protocol, class Executor,
    class EndpointSequence,
    class ConnectCondition,
    class RangeConnectHandler
#if ! BOOST_BEAST_DOXYGEN
    ,class = typename std::enable_if<
        net::is_endpoint_sequence<
            EndpointSequence>::value>::type
#endif
>
BOOST_ASIO_INITFN_RESULT_TYPE(RangeConnectHandler,
    void (error_code, typename Protocol::endpoint))
async_connect(
    stranded_socket<Protocol, Executor>& socket,
    EndpointSequence const& endpoints,
    ConnectCondition connect_condition,
    RangeConnectHandler&& handler)
{
    BOOST_BEAST_HANDLER_INIT(RangeConnectHandler,
        void(error_code, typename Protocol::endpoint));
    net::async_connect(socket.socket(),
        endpoints,
        connect_condition,
        detail::bind_default_executor(
            socket.get_executor(),
            std::move(init.completion_handler)));
    return init.result.get();
}

/** Asynchronously establishes a socket connection by trying each endpoint in a sequence, and terminating if a timeout occurs.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the underlying socket's
    @c async_connect member function, once for each endpoint in the sequence,
    until a connection is successfully established or a timeout occurs.
    
    @param socket The socket to be connected.
    If the underlying socket is already open, it will be closed.
    
    @param begin An iterator pointing to the start of a sequence of endpoints.
    
    @param end An iterator pointing to the end of a sequence of endpoints.

    @param handler The handler to be called when the connect operation
    completes. Ownership of the handler may be transferred. The function
    signature of the handler must be:
    @code
    void handler(
        // Result of operation. if the sequence is empty, set to
        // net::error::not_found. Otherwise, contains the
        // error from the last connection attempt.
        error_code const& error,
    
        // On success, an iterator denoting the successfully
        // connected endpoint. Otherwise, the end iterator.
        Iterator iterator
    );
    @endcode
    Regardless of whether the asynchronous operation completes immediately or
    not, the handler will not be invoked from within this function. Invocation
    of the handler will be performed in a manner equivalent to using
    `net::io_context::post()`.
*/
template<
    class Protocol, class Executor,
    class Iterator,
    class IteratorConnectHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(IteratorConnectHandler,
    void (error_code, Iterator))
async_connect(
    stranded_socket<Protocol, Executor>& socket,
    Iterator begin, Iterator end,
    IteratorConnectHandler&& handler)
{
    BOOST_BEAST_HANDLER_INIT(IteratorConnectHandler,
        void(error_code, Iterator));
    net::async_connect(socket.socket(),
        begin, end,
        detail::bind_default_executor(
            socket.get_executor(),
            std::move(init.completion_handler)));
    return init.result.get();
}

/** Asynchronously establishes a socket connection by trying each endpoint in a sequence, and terminating if a timeout occurs.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the underlying socket's
    @c async_connect member function, once for each endpoint in the sequence,
    until a connection is successfully established or a timeout occurs.
    
    @param socket The socket to be connected.
    If the underlying socket is already open, it will be closed.
    
    @param begin An iterator pointing to the start of a sequence of endpoints.

    @param end An iterator pointing to the end of a sequence of endpoints.
    
    @param connect_condition A function object that is called prior to each
    connection attempt. The signature of the function object must be:
    @code
    bool connect_condition(
        error_code const& ec,
        typename Protocol::endpoint const& next);
    @endcode
    The @c ec parameter contains the result from the most recent connect
    operation. Before the first connection attempt, @c ec is always set to
    indicate success. The @c next parameter is the next endpoint to be tried.
    The function object should return true if the next endpoint should be tried,
    and false if it should be skipped.

    @param handler The handler to be called when the connect operation
    completes. Ownership of the handler may be transferred. The function
    signature of the handler must be:
    @code
    void handler(
        // Result of operation. if the sequence is empty, set to
        // net::error::not_found. Otherwise, contains the
        // error from the last connection attempt.
        error_code const& error,
    
        // On success, an iterator denoting the successfully
        // connected endpoint. Otherwise, the end iterator.
        Iterator iterator
    );
    @endcode
    Regardless of whether the asynchronous operation completes immediately or
    not, the handler will not be invoked from within this function. Invocation
    of the handler will be performed in a manner equivalent to using
    `net::io_context::post()`.
*/
template<
    class Protocol, class Executor,
    class Iterator,
    class ConnectCondition,
    class IteratorConnectHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(IteratorConnectHandler,
    void (error_code, Iterator))
async_connect(
    stranded_socket<Protocol, Executor>& socket,
    Iterator begin, Iterator end,
    ConnectCondition connect_condition,
    IteratorConnectHandler&& handler)
{
    BOOST_BEAST_HANDLER_INIT(IteratorConnectHandler,
        void(error_code, Iterator));
    net::async_connect(socket.socket(),
        begin, end,
        connect_condition,
        detail::bind_default_executor(
            socket.get_executor(),
            std::move(init.completion_handler)));
    return init.result.get();
}

} // beast
} // boost

#endif
