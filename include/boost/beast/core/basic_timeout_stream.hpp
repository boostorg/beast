//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_BASIC_TIMEOUT_SOCKET_HPP
#define BOOST_BEAST_CORE_BASIC_TIMEOUT_SOCKET_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/timeout_stream_base.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/config/workaround.hpp>
#include <boost/optional.hpp>
#include <chrono>
#include <memory>

namespace boost {
namespace beast {

//------------------------------------------------------------------------------

/** A stream socket with an integrated timeout on reading, writing, and connecting.

    This layered stream wrapper manages a contained `net::basic_stream_socket`
    object to provide the following additional features:

    @li A timeout may be specified for each logical asynchronous
    operation that performs reading, writing, and/or connecting.

    @li The class template is parameterized on the executor type to be
    used for all asynchronous operations. This achieves partial support for
    <em>"Networking TS enhancement to enable custom I/O executors"</em>
    (<a href="http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1322r0.html">P1322R0</a>).

    Objects of this type are used in place of a regular networking socket,
    where timeouts on operations are desired. In particular this class
    template is designed to be used in place of `net::basic_stream_socket`
    or more typically, `net:ip::tcp::socket`. Constructors are provided to
    use a particular execution context or executor, subject to temporary
    restrictions based on the current implementation of networking. Additional
    constructors allow the timeout stream to be constructed from a networking
    socket that already exists.

    Although the stream supports multiple concurrent outstanding asynchronous
    operations, the stream object itself is not thread-safe. The caller is
    responsible for ensuring that the stream is accessed from only one thread
    at a time. This includes the times when the stream, and its underlying
    socket, is accessed by the networking implementation. To meet these
    thread safety requirements, all asynchronous operations must be performed
    by the stream within the same implicit strand (only one thread calling `run()`
    on the corresponding `net::io_context`) or within the same explicit strand
    such as an instance of `net::strand`.

    When using explicit strands, calls to initiating functions may use
    `net::bind_handler` with a suitable executor on the completion handler.
    Alternatively, the executor may be specified once by passing it as a stream
    class template parameter, and providing an instance of the executor upon
    construction (if the executor type is not <em>DefaultConstructible</em>).
    Regardless of the method chosen, the executor used with the stream must
    provide the following guarantees:

    @li <b>Ordering:</b>
    Function objects submitted to the executor from the same thread shall
    execute in the order they were submitted.

    @li <b>Concurrency:</b>
    Function objects submitted to the executor shall never run concurrently
    with each other.

    The executor type `net::strand` meets these requirements. Use of a
    strand as the executor in the stream class template offers an additional
    notational convenience: the strand does not need to be specified in
    each individual initiating function call.

    @par Usage

    To use this stream declare an instance of the class. Then, before
    each logical operation for which a timeout is desired, call
    @ref expires_after with a duration, or call @ref expires_at with a
    time point. Alternatively, call @ref expires_never to disable the
    timeout for subsequent logical operations. A logical operation
    is any series of one or more direct or indirect calls to the timeout
    stream's read, write, or connect functions.

    When a timeout is set and a mixed operation is performed (one that
    includes both reads and writes, for example) the timeout applies
    to all of the intermediate asynchronous operations used in the
    enclosing operation. This allows timeouts to be applied to stream
    algorithms which were not written specifically to allow for timeouts,
    when those algorithms are passed a timeout stream with a timeout set.

    When a timeout occurs the socket will be closed, canceling any
    pending I/O operations. The completion handlers for these canceled
    operations will be invoked with the error @ref beast::error::timeout.

    @par Examples

    This function reads an HTTP request with a timeout, then sends the
    HTTP response with a different timeout.

    @code
    void process_http_1 (timeout_stream& stream, net::yield_context yield)
    {
        flat_buffer buffer;
        http::request<http::empty_body> req;

        // Read the request, with a 15 second timeout
        stream.expires_after(std::chrono::seconds(15));
        http::async_read(stream, buffer, req, yield);

        // Calculate the response
        http::response<http::string_body> res = make_response(req);

        // Send the response, with a 30 second timeout.
        stream.expires_after (std::chrono::seconds(30));
        http::async_write (stream, res, yield);
    }
    @endcode

    The example above could be expressed using a single timeout with a
    simple modification. The function that follows first reads an HTTP
    request then sends the HTTP response, with a single timeout that
    applies to the entire combined operation of reading and writing:

    @code
    void process_http_2 (timeout_stream& stream, net::yield_context yield)
    {
        flat_buffer buffer;
        http::request<http::empty_body> req;

        // Require that the read and write combined take no longer than 30 seconds
        stream.expires_after(std::chrono::seconds(30));

        http::async_read(stream, buffer, req, yield);

        http::response<http::string_body> res = make_response(req);
        http::async_write (stream, res, yield);
    }
    @endcode

    Some stream algorithms, such as `ssl::stream::async_handshake` perform
    both reads and writes. A timeout set before calling the initiating function
    of such composite stream algorithms will apply to the entire composite
    operation. For example, a timeout may be set on performing the SSL handshake
    thusly:

    @code
    void do_ssl_handshake (net::ssl::stream<timeout_stream>& stream, net::yield_context yield)
    {
        // Require that the SSL handshake take no longer than 10 seconds
        stream.expires_after(std::chrono::seconds(10));

        stream.async_handshake(net::ssl::stream_base::client, yield);
    }
    @endcode

    @tparam Protocol A type meeting the requirements of <em>Protocol</em>
    representing the protocol the protocol to use for the basic stream socket.
    A common choice is `net::ip::tcp`.

    @tparam Executor A type meeting the requirements of <em>Executor</em> to
    be used for submitting all completion handlers which do not already have an
    associated executor. This type defaults to `net::io_context::executor_type`.

    @par Thread Safety
    <em>Distinct objects</em>: Safe.@n
    <em>Shared objects</em>: Unsafe. The application must also ensure
    that all asynchronous operations are performed within the same
    implicit or explicit strand.

    @see <em>"Networking TS enhancement to enable custom I/O executors"</em>
    (<a href="http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1322r0.html">P1322R0</a>).
*/
template<
    class Protocol,
    class Executor = typename
        net::basic_stream_socket<Protocol>::executor_type>
class basic_timeout_stream
#if ! BOOST_BEAST_DOXYGEN
    : private detail::timeout_stream_base
#endif
{
    using time_point = typename
        std::chrono::steady_clock::time_point;

    static constexpr time_point never()
    {
        return (time_point::max)();
    }

    static std::size_t constexpr no_limit =
        (std::numeric_limits<std::size_t>::max)();

    using tick_type = std::uint64_t;

    struct op_state
    {
        net::steady_timer timer;    // for timing out
        tick_type tick = 0;         // counts waits
        bool pending = false;       // if op is pending
        bool timeout = false;       // if timed out

        explicit
        op_state(net::io_context& ioc)
            : timer(ioc)
        {
        }
    };

// friend class template declaration in a class template is ignored
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=88672
#if BOOST_WORKAROUND(BOOST_GCC, > 0)
public:
#endif
    struct impl_type
        : std::enable_shared_from_this<impl_type>
    {
        Executor ex;                        // must come first
        op_state read;
        op_state write;

        net::basic_stream_socket<Protocol> socket;

        template<class... Args>
        explicit
        impl_type(Executor const&, Args&&...);

        impl_type(impl_type&&) = default;
        impl_type& operator=(impl_type&&);

        void reset();       // set timeouts to never
        void close();       // cancel everything
    };
#if BOOST_WORKAROUND(BOOST_GCC, > 0)
private:
#endif

    // We use shared ownership for the state so it can
    // outlive the destruction of the stream_socket object,
    // in the case where there is no outstanding read or
    // write but the implementation is still waiting on
    // the rate timer.
    std::shared_ptr<impl_type> impl_;

    // Restricted until P1322R0 is incorporated into Boost.Asio.
    static_assert(
        std::is_convertible<
            decltype(std::declval<Executor const&>().context()),
            net::io_context&>::value,
        "Only net::io_context is currently supported for executor_type::context()");

    template<bool, class, class> class async_op;

    template<class, class, class>
    friend class detail::timeout_stream_connect_op;

    template<class, class>
    friend class basic_timeout_stream;

    struct timeout_handler;

public:
    /// The type of the executor associated with the object.
    using executor_type = Executor;

    /// The type of the next layer.
    using next_layer_type = net::basic_stream_socket<Protocol>;

    /// The protocol type.
    using protocol_type = Protocol;

    /// The endpoint type.
    using endpoint_type = typename Protocol::endpoint;

    /** Destructor

        This function destroys the socket, cancelling any outstanding
        asynchronous operations associated with the socket as if by
        calling cancel.
    */
    ~basic_timeout_stream();

    /** Construct the stream without opening it.

        This constructor creates a timeout stream. The underlying socket needs
        to be opened and then connected or accepted before data can be sent or
        received on it.

        @param ctx An object whose type meets the requirements of
        <em>ExecutionContext</em>, which the stream will use to dispatch
        handlers for any asynchronous operations performed on the socket.
        Currently, the only supported type for `ctx` is `net::io_context`.

        @note This function does not participate in overload resolution unless:
        @li `std::is_convertible<ExecutionContext&, net::execution_context&>::value` is `true`, and
        @li `std::is_constructible<executor_type, typename ExecutionContext::executor_type>::value` is `true`.

        @see http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1322r0.html
    */
    template<
        class ExecutionContext
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
    basic_timeout_stream(ExecutionContext& ctx);

    /** Construct the stream without opening it.

        This constructor creates a timeout stream. The underlying socket needs
        to be opened and then connected or accepted before data can be sent or
        received on it.

        @param ex The executor which stream will use to dispatch handlers for
        any asynchronous operations performed on the underlying socket.
        Currently, only executors that return a `net::io_context&` from
        `ex.context()` are supported.

        @see http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1322r0.html
    */
    explicit
    basic_timeout_stream(executor_type const& ex);

    /** Construct the stream with an existing socket.

        This constructor creates a timeout stream by taking ownership of an
        already existing socket. The executor will be the same as the executor
        of the provided socket.

        @param socket The socket object to construct with, which becomes the
        next layer of the timeout stream. Ownership of this socket is
        transferred by move.

        @see http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1322r0.html
    */
    explicit
    basic_timeout_stream(
        net::basic_stream_socket<Protocol>&& socket);

    /** Construct the stream with an executor and existing socket.

        This constructor creates a timeout stream by taking ownership of an
        already existing socket.

        @param ex The executor which stream will use to dispatch handlers for
        any asynchronous operations performed on the underlying socket.
        Currently, only executors that return a `net::io_context&` from
        `ex.context()` are supported.

        @param socket The socket object to construct with, which becomes the
        next layer of the timeout stream. Ownership of this socket is
        transferred by move.

        @see http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1322r0.html
    */
    basic_timeout_stream(
        executor_type const& ex,
        net::basic_stream_socket<Protocol>&& socket);

    /** Move-construct a stream from another stream

        This constructor moves a stream from one object to another.

        The behavior of moving a stream while asynchronous operations
        are outstanding is undefined.

        @param other The other object from which the move will occur. 

        @note Following the move, the moved-from object is in a newly
        constructed state.
    */
    basic_timeout_stream(basic_timeout_stream&& other);

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
    basic_timeout_stream& operator=(basic_timeout_stream&& other);

    //--------------------------------------------------------------------------

    /** Get the executor associated with the object.
    
        This function may be used to obtain the executor object that the
        stream uses to dispatch handlers for asynchronous operations.

        @return A copy of the executor that stream will use to dispatch handlers.
    */
    executor_type
    get_executor() const noexcept
    {
        return impl_->ex;
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
        return impl_->socket;
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
        return impl_->socket;
    }

    /** Set the timeout for the next logical operation.

        This sets either the read timer, the write timer, or
        both timers to expire after the specified amount of time
        has elapsed. If a timer expires when the corresponding
        asynchronous operation is outstanding, the stream will be
        closed and any outstanding operations will complete with the
        error @ref beast::error::timeout. Otherwise, if the timer
        expires while no operations are outstanding, and the expiraton
        is not set again, the next operation will time out immediately.

        The timer applies collectively to any asynchronous reads
        or writes initiated after the expiration is set, until the
        expiration is set again. A call to @ref beast::async_connect
        counts as both a read and a write.

        @param expiry_time The amount of time after which a logical
        operation should be considered timed out.
    */
    void
    expires_after(
        std::chrono::nanoseconds expiry_time);

    /** Set the timeout for the next logical operation.

        This sets either the read timer, the write timer, or both
        timers to expire at the specified time point. If a timer
        expires when the corresponding asynchronous operation is
        outstanding, the stream will be closed and any outstanding
        operations will complete with the error @ref beast::error::timeout.
        Otherwise, if the timer expires while no operations are outstanding,
        and the expiraton is not set again, the next operation will time out
        immediately.

        The timer applies collectively to any asynchronous reads
        or writes initiated after the expiration is set, until the
        expiration is set again. A call to @ref beast::async_connect
        counts as both a read and a write.

        @param expiry_time The time point after which a logical
        operation should be considered timed out.
    */
    void
    expires_at(net::steady_timer::time_point expiry_time);

    /// Disable the timeout for the next logical operation.
    void
    expires_never();

    /// Cancel all asynchronous operations associated with the socket.
    void
    cancel();

    /** Close the timed stream.

        This cancels all timers and pending I/O. The completion handlers
        for any pending I/O will see an error code.
    */
    void
    close();

    //--------------------------------------------------------------------------

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
        ReadHandler&& handler);

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
        WriteHandler&& handler);
};

//------------------------------------------------------------------------------

/** Asynchronously establishes a socket connection by trying each endpoint in a sequence, and terminating if a timeout occurs.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the underlying socket's
    @c async_connect member function, once for each endpoint in the sequence,
    until a connection is successfully established or a timeout occurs.
    
    @param stream The @ref beast::basic_timeout_stream to be connected. If the
    underlying socket is already open, it will be closed.
    
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
    basic_timeout_stream<Protocol, Executor>& stream,
    EndpointSequence const& endpoints,
    RangeConnectHandler&& handler);

/** Asynchronously establishes a socket connection by trying each endpoint in a sequence, and terminating if a timeout occurs.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the underlying socket's
    @c async_connect member function, once for each endpoint in the sequence,
    until a connection is successfully established or a timeout occurs.
    
    @param stream The @ref beast::basic_timeout_stream to be connected. If the
    underlying socket is already open, it will be closed.

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
    basic_timeout_stream<Protocol, Executor>& stream,
    EndpointSequence const& endpoints,
    ConnectCondition connect_condition,
    RangeConnectHandler&& handler);

/** Asynchronously establishes a socket connection by trying each endpoint in a sequence, and terminating if a timeout occurs.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the underlying socket's
    @c async_connect member function, once for each endpoint in the sequence,
    until a connection is successfully established or a timeout occurs.
    
    @param stream The @ref beast::basic_timeout_stream to be connected. If the
    underlying socket is already open, it will be closed.
    
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
    basic_timeout_stream<Protocol, Executor>& stream,
    Iterator begin, Iterator end,
    IteratorConnectHandler&& handler);

/** Asynchronously establishes a socket connection by trying each endpoint in a sequence, and terminating if a timeout occurs.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the underlying socket's
    @c async_connect member function, once for each endpoint in the sequence,
    until a connection is successfully established or a timeout occurs.
    
    @param stream The @ref beast::basic_timeout_stream to be connected. If the
    underlying socket is already open, it will be closed.
    
    @param begin An iterator pointing to the start of a sequence of endpoints.

    @param end An iterator pointing to the end of a sequence of endpoints.
    
    @param connect_condition A function object that is called prior to each
    connection attempt. The signature of the function object must be:
    @code
    bool connect_condition(
        error_code const& ec,
        Iterator next);
    @endcode
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
    basic_timeout_stream<Protocol, Executor>& stream,
    Iterator begin, Iterator end,
    ConnectCondition connect_condition,
    IteratorConnectHandler&& handler);

} // beast
} // boost

#include <boost/beast/core/impl/basic_timeout_stream.hpp>

#endif
