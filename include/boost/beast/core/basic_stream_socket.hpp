//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_BASIC_STREAM_SOCKET_HPP
#define BOOST_BEAST_CORE_BASIC_STREAM_SOCKET_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/stream_socket_base.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/type_traits.hpp>
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

/** A stream socket with integrated timeout and bandwidth management.

    This stream socket adapts a `net::basic_stream_socket` to provide
    the following additional features:

    @li The class template is parameterized on a user-defined executor
    used for asynchronous operations. This achieves partial support for
    "Networking TS enhancement to enable custom I/O executors"
    (<a href="http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1322r0.html">P1322R0</a>).

    @li Optional timeouts may be specified for logical operations which
    perform asynchronous reads, writes, and connects.

    @li Optional bytes-per-second rate limits may be set independently
    on asynchronous reads and writes.

    @par Usage

    Objects of this type are designed to be used in places where a
    regular networking TCP/IP socket is being used. In particular this
    class template replaces `net::basic_stream_socket`. The constructors
    used here are similar to those of networking sockets, but with the
    ability to use either an executor or an execution context when
    constructing the socket.

    The caller is responsible for ensuring that all stream operations,
    including the internal timer operations, are running from the same
    implicit or explicit strand. When there are multiple threads calling
    `net::io_context::run`, the `Executor` template parameter, and the
    instance of that executor type passed to the constructor must provide
    the following guarantees:

    @li <b>Serial Execution:</b>
    Function objects submitted to the executor shall never run
    concurrently.

    @li <b>Ordering:</b>
    Function objects submitted to the executor from the same thread shall
    execute in the order they were submitted.

    If only one thread is calling `net::io_context::run`, the executor
    may be the executor used by the I/O context (`net::io_context::executor_type`).
    Otherwise, a `net::strand` may be used to meet the requirements.
    Note that when using a strand to instantiate the socket, it is not
    necessary to also bind each submitted completion handler used in
    subsequent operations to the strand, this is taken care of automatically.

    @par Associated Executor


    @par Using Timeouts

    To use this stream declare an instance of the class. Then, before
    each logical operation for which a timeout is desired, call
    @ref expires_after with a duration, or call @ref expires_at with a
    time point. Alternatively, call @ref expires_never to disable the
    timeout for subsequent logical operations.

    A logical operation is defined as one of the following:
    
    @li A call to @ref beast::async_connect where the stream is passed
    as the first argument.

    @li One or more calls to either one or both of the stream's
    @ref async_read_some and @ref async_write_some member functions.
    This also includes indirect calls, for example when passing the
    stream as the first argument to an initiating function such as
    `net::async_read_until`.

    Each logical operation can be considered as peforming just reads,
    just writes, or both reads and writes. Calls to @ref beast::async_connect
    count as both a read and a write, although no actual reads or writes
    are performed. While logical operations can include both reading
    and writing, the usual restriction on having at most one read and
    one write outstanding simultaneously applies.

    The implementation maintains two timers: one timer for reads, and
    another timer for writes. When the expiration time is adjusted
    (by calling @ref expires_after or @ref expires_at), the indiviudal
    timer is only set if there is not currently an operation of that
    type (read or write) outstanding. It is undefined behavior to set
    an expiration when there is both a read and a write pending, since
    there would be no available timer to apply the expiration to.

    @par Example
    This code sets a timeout, and uses a generic networking stream algorithm
    to read data from a timed stream until a particular delimiter is found
    or until the stream times out:
    @code
    template <class Protocol, class Executor, class ReadHandler>
    void async_read_line (
        basic_stream_socket<Protocol, Executor>& stream,
        net::streambuf& buffer, ReadHandler&& handler)
    {
        stream.expires_after (std::chrono::seconds(30));

        net::async_read_until (stream, buffer, "\r\n", std::forward<ReadHandler>(handler));
    }
    @endcode

    When a timeout occurs the socket will be closed, canceling any
    pending I/O operations. The completion handlers for these canceled
    operations will be invoked with the error @ref beast::error::timeout.

    @par Using Rate Limits

    @tparam Protocol A type meeting the requirements of <em>Protocol</em>
    representing the protocol the protocol to use for the basic stream socket.
    A common choice is `net::ip::tcp`.

    @tparam Executor A type meeting the requirements of <em>Executor</em> to
    be used for submitting all completion handlers which do not already have an
    associated executor.

    @note A multi-stream object must not be moved or destroyed while there
    are oustanding asynchronous operations associated with it. Objects of this
    type meet the requirements of @b AsyncReadStream and @b AsyncWriteStream.

    @par Thread Safety
    <em>Distinct objects</em>: Safe.@n
    <em>Shared objects</em>: Unsafe. The application must also ensure
    that all asynchronous operations are performed within the same
    implicit or explicit strand.

    @see "Networking TS enhancement to enable custom I/O executors"
    (<a href="http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1322r0.html">P1322R0</a>).
*/
template<class Protocol, class Executor>
class basic_stream_socket
    : private detail::stream_socket_base
{
    using time_point = typename
        std::chrono::steady_clock::time_point;

    // the number of seconds in each time slice
    // for applying bandwidth rate limiting.
    enum : std::size_t
    {
        rate_seconds = 3
    };

    static constexpr time_point never()
    {
        return (time_point::max)();
    }

    static std::size_t constexpr no_limit =
        (std::numeric_limits<std::size_t>::max)();

// friend class template declaration in a class template is ignored
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=88672
#if BOOST_WORKAROUND(BOOST_GCC, > 0)
public:
#endif
    struct impl_type
        : std::enable_shared_from_this<impl_type>
    {
        Executor ex;                        // must come first
        net::basic_stream_socket<
            Protocol> socket;
        net::steady_timer rate_timer;       // rate-limit interval timer
        net::steady_timer read_timer;       // for read timeout
        net::steady_timer write_timer;      // for write/connect timeout

        // VFALCO these could be 32-bit unsigneds
        std::size_t read_limit = no_limit;  // read budget
        std::size_t read_remain = no_limit; // read balance
        std::size_t write_limit = no_limit; // write budget
        std::size_t write_remain = no_limit;// write balance

        char waiting = 0;                   // number of waiters on rate timer
        bool read_pending = false;          // if read (or connect) is pending
        bool read_closed = false;           // if read timed out
        bool write_pending = false;         // if write (or connect) is pending
        bool write_closed = false;          // if write (or connect) timed out

        template<class... Args>
        explicit
        impl_type(Executor const&, Args&&...);

        impl_type(impl_type&&) = default;
        impl_type& operator=(impl_type&&);

        void reset();       // set timeouts to never
        void close();       // cancel everything
        void maybe_kick();  // kick the rate timer if needed
        void on_timer();    // rate timer completion

        using executor_type = Executor;
        Executor
        get_executor() const noexcept
        {
            return ex;
        }
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

    template<class, class> class read_op;
    template<class, class> class write_op;

    template<class, class, class>
    friend class detail::stream_socket_connect_op;

    template<class, class>
    friend class basic_stream_socket;

    struct read_timeout_handler;
    struct write_timeout_handler;

public:
    /// The type of the next layer.
    using next_layer_type = net::basic_stream_socket<Protocol>;

    /// The type of the lowest layer.
    using lowest_layer_type = get_lowest_layer<next_layer_type>;

    /// The type of the executor associated with the object.
    using executor_type = Executor;

    /// The protocol type.
    using protocol_type = Protocol;

    /// The endpoint type.
    using endpoint_type = typename Protocol::endpoint;

    /** Destructor

        This function destroys the socket.

        @note The behavior of destruction while asynchronous
        operations are outstanding is undefined.
    */
    ~basic_stream_socket();

    /** Construct a basic_stream_socket without opening it.

        This constructor creates a stream socket without opening it. The
        socket needs to be opened and then connected or accepted before
        data can be sent or received on it.

        @param ctx An object whose type meets the requirements of
        <em>ExecutionContext</em>, which the stream socket will use
        to dispatch handlers for any asynchronous operations performed
        on the socket. Currently, the only supported execution context
        which may be passed here is `net::io_context`.

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
    basic_stream_socket(ExecutionContext& ctx);

    /** Construct a basic_stream_socket without opening it.

        This constructor creates a stream socket without opening it. The
        socket needs to be opened and then connected or accepted before
        data can be sent or received on it.

        @param ex The executor which the stream socket will use to dispatch
        handlers for any asynchronous operations performed on the socket.
        Currently, only executors that return `net::io_context&` from
        `ex.context()` are supported.

        @see http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1322r0.html
    */
    explicit
    basic_stream_socket(executor_type const& ex);

    /** Construct and open a basic_stream_socket.

        This constructor creates and opens a stream socket. The socket
        needs to be connected or accepted before data can be sent or
        received on it.

        @param ctx An object whose type meets the requirements of
        <em>ExecutionContext</em>, which the stream socket will use
        to dispatch handlers for any asynchronous operations performed
        on the socket. Currently, the only supported execution context
        which may be passed here is `net::io_context`.

        @param protocol An object specifying protocol parameters to be
        used.

        @note This function does not participate in overload resolution unless:
        @li `std::is_convertible<ExecutionContext&, net::execution_context&>::value` is `true`, and
        @li `std::is_constructible<executor_type, typename ExecutionContext::executor_type>::value` is `true`.

        @see http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1322r0.html
    */
    template<
        class ExecutionContext
    #if ! BOOST_BEAST_DOXYGEN
        ,class = typename std::enable_if<
        std::is_convertible<
            ExecutionContext&,
            net::execution_context&>::value &&
        std::is_constructible<
            executor_type,
            typename ExecutionContext::executor_type>::value
        >::type
    #endif
    >
    basic_stream_socket(
        ExecutionContext& ctx,
        protocol_type const& protocol);

    /** Construct and open a basic_stream_socket.

        This constructor creates and opens a stream socket. The socket
        needs to be connected or accepted before data can be sent or
        received on it.

        @param ex The executor which the stream socket will use to dispatch
        handlers for any asynchronous operations performed on the socket.
        Currently, only executors that return `net::io_context&` from
        `ex.context()` are supported.

        @param protocol An object specifying protocol parameters to be
        used.

        @note This function does not participate in overload resolution unless:
        @li `std::is_convertible<ExecutionContext&, net::execution_context&>::value` is `true`, and
        @li `std::is_constructible<executor_type, typename ExecutionContext::executor_type>::value` is `true`.

        @see http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1322r0.html
    */
    basic_stream_socket(
        executor_type const& ex,
        protocol_type const& protocol);

    /** Construct a basic_stream_socket, opening and binding it to the given local endpoint.

        This constructor creates a stream socket and automatically
        opens it bound to the specified endpoint on the local machine.
        The protocol used is the protocol associated with the given
        endpoint.

        @param ctx An object whose type meets the requirements of
        <em>ExecutionContext</em>, which the stream socket will use
        to dispatch handlers for any asynchronous operations performed
        on the socket. Currently, the only supported execution context
        which may be passed here is `net::io_context`.

        @param endpoint An endpoint on the local machine to which the
        stream socket will be bound.

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
    basic_stream_socket(
        ExecutionContext& ctx,
        endpoint_type const& endpoint);

    /** Construct a basic_stream_socket, opening and binding it to the given local endpoint.

        This constructor creates a stream socket and automatically
        opens it bound to the specified endpoint on the local machine.
        The protocol used is the protocol associated with the given
        endpoint.

        @param ex The executor which the stream socket will use to dispatch
        handlers for any asynchronous operations performed on the socket.
        Currently, only executors that return `net::io_context&` from
        `ex.context()` are supported.

        @param endpoint An endpoint on the local machine to which the
        stream socket will be bound.

        @note This function does not participate in overload resolution unless:
        @li `std::is_convertible<ExecutionContext&, net::execution_context&>::value` is `true`, and
        @li `std::is_constructible<executor_type, typename ExecutionContext::executor_type>::value` is `true`.

        @see http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1322r0.html
    */
    basic_stream_socket(
        executor_type const& ex,
        endpoint_type const& endpoint);

    /** Construct a basic_stream_socket, opening and binding it to the given local endpoint.

        This constructor creates a stream socket object to from an existing
        next layer object.

        @param ctx An object whose type meets the requirements of
        <em>ExecutionContext</em>, which the stream socket will use
        to dispatch handlers for any asynchronous operations performed
        on the socket. Currently, the only supported execution context
        which may be passed here is `net::io_context`.

        @param socket The socket object to construct with. Ownership of
        this object is transferred by move.

        @note This function does not participate in overload resolution unless:
        @li `std::is_convertible<ExecutionContext&, net::execution_context&>::value` is `true`, and
        @li `std::is_constructible<executor_type, typename ExecutionContext::executor_type>::value` is `true`.

        @see http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1322r0.html
    */
    template<
        class ExecutionContext
    #if ! BOOST_BEAST_DOXYGEN
        ,class = typename std::enable_if<
        std::is_convertible<
            ExecutionContext&,
            net::execution_context&>::value &&
        std::is_constructible<
            executor_type,
            typename ExecutionContext::executor_type>::value
        >::type
    #endif
    >
    basic_stream_socket(
        ExecutionContext& ctx,
        next_layer_type&& socket);

    /** Construct a basic_stream_socket, opening and binding it to the given local endpoint.

        This constructor creates a stream socket object to from an existing
        next layer object.

        @param ex The executor which the stream socket will use to dispatch
        handlers for any asynchronous operations performed on the socket.
        Currently, only executors that return `net::io_context&` from
        `ex.context()` are supported.

        @param socket The socket object to construct with. Ownership of
        this object is transferred by move.

        @note This function does not participate in overload resolution unless:
        @li `std::is_convertible<ExecutionContext&, net::execution_context&>::value` is `true`, and
        @li `std::is_constructible<executor_type, typename ExecutionContext::executor_type>::value` is `true`.

        @see http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1322r0.html
    */
    basic_stream_socket(
        executor_type const& ex,
        next_layer_type&& socket);

    /** Move-construct a basic_stream_socket from another

        This constructor moves a stream socket from one object to another.

        The behavior of moving a stream socket while asynchronous operations
        are outstanding is undefined.

        @param other The other basic_stream_socket object from which the
        move will occur. 

        @note Following the move, the moved-from object is in the same state
        as if constructed using the @c basic_stream_socket(ExecutionContext&)
        constructor.
    */
    basic_stream_socket(basic_stream_socket&& other);

    /** Move-assign a basic_stream_socket from another.

        This assignment operator moves a stream socket from one object
        to another.

        The behavior of move assignment while asynchronous operations
        are pending is undefined.

        @param other The other basic_stream_socket object from which the
        move will occur. 

        @note Following the move, the moved-from object is in the same state
        as if constructed using the @c basic_stream_socket(ExecutionContext&)
        constructor.
    */
    basic_stream_socket& operator=(basic_stream_socket&& other);

    /** Move-construct a basic_stream_socket from a socket of another protocol and executor type.

        This constructor moves a stream socket from one object to another.

        The behavior of moving a stream socket while asynchronous operations
        are outstanding is undefined.

        @param other The other basic_stream_socket object from which the
        move will occur. 

        @note This constructor does not participate in overload resolution unless:
        @li `std::is_convertible<OtherProtocol, protocol_type>::value` is `true`, and
        @li `std::is_convertible<OtherExecutor, executor_type>::value` is `true`.
    */
    template<
        class OtherProtocol,
        class OtherExecutor
    #if ! BOOST_BEAST_DOXYGEN
        ,class = typename std::enable_if<
            std::is_convertible<
                OtherProtocol, protocol_type>::value &&
            std::is_convertible<
                OtherExecutor, executor_type>::value>::type
    #endif
        >
    basic_stream_socket(
        basic_stream_socket<OtherProtocol, OtherExecutor>&& other);

    /** Move-assign a basic_stream_socket from a socket of another protocol and executor type.

        This assignment operator a stream socket from one object to another.

        The behavior of moving a stream socket while asynchronous operations
        are outstanding is undefined.

        @param other The other basic_stream_socket object from which the
        move will occur. 

        @note This constructor does not participate in overload resolution unless:
        @li `std::is_convertible<OtherProtocol, protocol_type>::value` is `true`, and
        @li `std::is_convertible<OtherExecutor, executor_type>::value` is `true`.
    */
    template<
        class OtherProtocol,
        class OtherExecutor
    #if ! BOOST_BEAST_DOXYGEN
        ,class = typename std::enable_if<
            std::is_convertible<
                OtherProtocol, protocol_type>::value &&
            std::is_convertible<
                OtherExecutor, executor_type>::value>::type
    #endif
        >
    basic_stream_socket& operator=(basic_stream_socket<
        OtherProtocol, OtherExecutor>&& other);

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

    /** Get a reference to the next layer

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

    /** Get a reference to the next layer

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

    /** Get a reference to the lowest layer

        This function returns a reference to the lowest layer
        in a stack of stream layers.

        @return A reference to the lowest layer in the stack of
        stream layers.
    */
    lowest_layer_type&
    lowest_layer() noexcept
    {
        return impl_->socket.lowest_layer();
    }

    /** Get a reference to the lowest layer

        This function returns a reference to the lowest layer
        in a stack of stream layers.

        @return A reference to the lowest layer in the stack of
        stream layers. Ownership is not transferred to the caller.
    */
    lowest_layer_type const&
    lowest_layer() const noexcept
    {
        return impl_->socket.lowest_layer();
    }

    /** Set the number of bytes allowed to be read per second.

        The limit will take effect in the next measured time
        interval (currently set to 3 seconds).

        @param bytes_per_second The maximum number of bytes the
        implementation should attempt to read per second. A value
        of zero indicates no limit.
    */
    void
    read_limit(std::size_t bytes_per_second);

    /** Set the number of bytes allowed to be written per second.

        The limit will take effect in the next measured time
        interval (currently set to 3 seconds).

        @param bytes_per_second The maximum number of bytes the
        implementation should attempt to write per second. A value
        of zero indicates no limit.
    */
    void
    write_limit(std::size_t bytes_per_second);

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

    /** Close the timed stream.

        This cancels all timers and pending I/O. The completion handlers
        for any pending I/O will see an error code.
    */
    void
    close()
    {
        impl_->close();
    }

    //--------------------------------------------------------------------------

    /** Start an asynchronous read.

        This function is used to asynchronously read data from the stream socket.
        The function call always returns immediately.
        
        @param buffers One or more buffers into which the data will be read.
        Although the buffers object may be copied as necessary, ownership of the
        underlying memory blocks is retained by the caller, which must guarantee
        that they remain valid until the handler is called.
        
        @param handler The handler to be called when the read operation completes.
        Copies will be made of the handler as required. The function signature of
        the handler must be:
        @code
        void handler(
            error_code const & error,         // Result of operation.
            std::size_t bytes_transferred     // Number of bytes read.
        );
        @endcode
        Regardless of whether the asynchronous operation completes immediately or
        not, the handler will not be invoked from within this function. Invocation
        of the handler will be performed in a manner equivalent to using
        `net::io_context::post()`.
    */
    template<class MutableBufferSequence, class ReadHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(ReadHandler,
        void(error_code, std::size_t))
    async_read_some(
        MutableBufferSequence const& buffers,
        ReadHandler&& handler);

    /** Start an asynchronous write.

        This function is used to asynchronously write data to the stream socket.
        The function call always returns immediately.
        
        @param buffers One or more data buffers to be written to the socket.
        Although the buffers object may be copied as necessary, ownership of the
        underlying memory blocks is retained by the caller, which must guarantee
        that they remain valid until the handler is called.
        
        @param handler The handler to be called when the write operation completes.
        Copies will be made of the handler as required. The function signature of
        the handler must be:
        @code
        void handler(
            error_code const & error,         // Result of operation.
            std::size_t bytes_transferred     // Number of bytes written.
        );
        @endcode
        Regardless of whether the asynchronous operation completes immediately or
        not, the handler will not be invoked from within this function. Invocation
        of the handler will be performed in a manner equivalent to using
        `net::io_context::post()`.
    */
    template<class ConstBufferSequence, class WriteHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(WriteHandler,
        void(error_code, std::size_t))
    async_write_some(
        ConstBufferSequence const& buffers,
        WriteHandler&& handler);
};

//------------------------------------------------------------------------------

/**
    @defgroup async_connect boost::beast::async_connect
    @brief Asynchronously establishes a socket connection by trying each
        endpoint in a sequence, and terminating if a timeout occurs.
*/
/* @{ */
/** Asynchronously establishes a socket connection by trying each endpoint in a sequence, and terminating if a timeout occurs.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the underlying socket's
    @c async_connect member function, once for each endpoint in the sequence,
    until a connection is successfully established or a timeout occurs.
    
    @param s The @ref beast::basic_stream_socket to be connected. If the socket
    is already open, it will be closed.
    
    @param endpoints A sequence of endpoints.
    
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
    @code
    net::tcp::resolver r(ioc);
    net::tcp::resolver::query q("host", "service");
    timed_stream s(ioc.get_executor());
    
    // ...
    r.async_resolve(q, resolve_handler);
    
    // ...
    
    void resolve_handler(
        error_code const& ec,
        tcp::resolver::results_type results)
    {
        if (!ec)
        {
            async_connect(s, results, connect_handler);
        }
    }
    
    // ...
    void connect_handler(
        error_code const& ec,
        tcp::endpoint const& endpoint)
    {
        // ...
    }
    @endcode
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
    basic_stream_socket<Protocol, Executor>& s,
    EndpointSequence const& endpoints,
    RangeConnectHandler&& handler);

/** Asynchronously establishes a socket connection by trying each endpoint in a sequence, and terminating if a timeout occurs.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the underlying socket's
    @c async_connect member function, once for each endpoint in the sequence,
    until a connection is successfully established or a timeout occurs.
    
    @param s The @ref beast::basic_stream_socket to be connected. If the socket
    is already open, it will be closed.
    
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
            if (ec) std::cout << "Error: " << ec.message() << std::endl;
            std::cout << "Trying: " << next << std::endl;
            return true;
        }
    };
    @endcode
    It would be used with the @ref boost::beast::async_connect
    function as follows:
    @code
    net::tcp::resolver r(ioc);
    net::tcp::resolver::query q("host", "service");
    timed_stream s(ioc.get_executor());
    
    // ...
    r.async_resolve(q, resolve_handler);
    
    // ...
    
    void resolve_handler(
        error_code const& ec,
        tcp::resolver::results_type results)
    {
        if (!ec)
        {
            async_connect(s, results, my_connect_condition{}, connect_handler);
        }
    }
    
    // ...
    void connect_handler(
        error_code const& ec,
        tcp::endpoint const& endpoint)
    {
        // ...
    }
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
    basic_stream_socket<Protocol, Executor>& s,
    EndpointSequence const& endpoints,
    ConnectCondition connect_condition,
    RangeConnectHandler&& handler);

/** Asynchronously establishes a socket connection by trying each endpoint in a sequence, and terminating if a timeout occurs.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the underlying socket's
    @c async_connect member function, once for each endpoint in the sequence,
    until a connection is successfully established or a timeout occurs.
    
    @param s The @ref beast::basic_stream_socket to be connected. If the socket
    is already open, it will be closed.
    
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
    
    @par Example
    @code
    std::vector<tcp::endpoint> endpoints = ...;
    timed_stream s(ioc.get_executor());
        
    async_connect(s,
        endpoints.begin(), endpoints.end(),
        connect_handler);
    void connect_handler(
        error_code const& ec,
        std::vector<tcp::endpoint>::iterator)
    {
        // ...
    }
    @endcode
*/
template<
    class Protocol, class Executor,
    class Iterator,
    class IteratorConnectHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(IteratorConnectHandler,
    void (error_code, Iterator))
async_connect(
    basic_stream_socket<Protocol, Executor>& s,
    Iterator begin, Iterator end,
    IteratorConnectHandler&& handler);

/** Asynchronously establishes a socket connection by trying each endpoint in a sequence, and terminating if a timeout occurs.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the underlying socket's
    @c async_connect member function, once for each endpoint in the sequence,
    until a connection is successfully established or a timeout occurs.
    
    @param s The @ref beast::basic_stream_socket to be connected. If the socket
    is already open, it will be closed.
    
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
            if (ec) std::cout << "Error: " << ec.message() << std::endl;
            std::cout << "Trying: " << next << std::endl;
            return true;
        }
    };
    @endcode
    It would be used with the @ref boost::beast::async_connect
    function as follows:
    @code
    std::vector<tcp::endpoint> endpoints = ...;
    timed_stream s(ioc.get_executor());
        
    async_connect(s, endpoints.begin(), endpoints.end(),
        my_connect_condition{}, connect_handler);
    void connect_handler(
        error_code const& ec,
        std::vector<tcp::endpoint>::iterator)
    {
        // ...
    }
    @endcode
*/
template<
    class Protocol, class Executor,
    class Iterator,
    class ConnectCondition,
    class IteratorConnectHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(IteratorConnectHandler,
    void (error_code, Iterator))
async_connect(
    basic_stream_socket<Protocol, Executor>& s,
    Iterator begin, Iterator end,
    ConnectCondition connect_condition,
    IteratorConnectHandler&& handler);
/* @} */

} // beast
} // boost

#include <boost/beast/core/impl/basic_stream_socket.hpp>

#endif
