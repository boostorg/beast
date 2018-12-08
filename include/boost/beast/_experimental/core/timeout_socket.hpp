//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_TIMEOUT_SOCKET_HPP
#define BOOST_BEAST_CORE_TIMEOUT_SOCKET_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/_experimental/core/timeout_service.hpp>
#include <boost/beast/_experimental/core/detail/saved_handler.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/executor.hpp>
#include <chrono>

namespace boost {
namespace asio {
namespace ip {
class tcp;
} // ip
} // asio
} // boost

namespace boost {
namespace beast {

namespace detail {
template<class, class, class>
class connect_op;
} // detail

/** A socket wrapper which automatically times out on asynchronous reads.

    This wraps a normal stream socket and implements a simple and efficient
    timeout for asynchronous read operations.

    @note Meets the requirements of @b AsyncReadStream and @b AsyncWriteStream
*/
template<
    class Protocol,
    class Executor = net::executor
>
class basic_timeout_socket
{
    template<class> class async_op;
    template<class, class, class>
    friend class detail::connect_op;

    Executor ex_; // must come first
    timeout_handle rd_timer_;
    timeout_handle wr_timer_;
    timeout_handle cn_timer_;
    detail::saved_handler rd_op_;
    detail::saved_handler wr_op_;
    detail::saved_handler cn_op_;
    net::basic_stream_socket<Protocol> sock_;

public:
    /// The type of the next layer.
    using next_layer_type = net::basic_stream_socket<Protocol>;

    /// The type of the lowest layer.
    using lowest_layer_type = get_lowest_layer<next_layer_type>;

    /// The protocol used by the stream.
    using protocol_type = Protocol;

    /// The type of the executor associated with the object.
    using executor_type = Executor;

    /** Destructor

        The behavior of destruction while asynchronous operations
        are pending is undefined.
    */
    ~basic_timeout_socket();

    // VFALCO we only support default-construction
    //        of the contained socket for now.
    //        This constructor needs a protocol parameter.
    //
    /** Constructor
    */
    template<class ExecutionContext
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
    basic_timeout_socket(ExecutionContext& ctx);
 
    //--------------------------------------------------------------------------

    /** Get the executor associated with the object.
    
        This function may be used to obtain the executor object that the
        stream uses to dispatch handlers for asynchronous operations.

        @return A copy of the executor that stream will use to dispatch handlers.
    */
    executor_type
    get_executor() const noexcept
    {
        return ex_;
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
        return sock_;
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
        return sock_;
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
        return sock_.lowest_layer();
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
        return sock_.lowest_layer();
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
        @code void handler(
          const boost::system::error_code& error, // Result of operation.
          std::size_t bytes_transferred           // Number of bytes read.
        ); @endcode
        Regardless of whether the asynchronous operation completes immediately or
        not, the handler will not be invoked from within this function. Invocation
        of the handler will be performed in a manner equivalent to using
        net::io_context::post().
    */
    template<class MutableBufferSequence, class ReadHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(ReadHandler,
        void(boost::system::error_code, std::size_t))
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
        @code void handler(
          const boost::system::error_code& error, // Result of operation.
          std::size_t bytes_transferred           // Number of bytes written.
        ); @endcode
        Regardless of whether the asynchronous operation completes immediately or
        not, the handler will not be invoked from within this function. Invocation
        of the handler will be performed in a manner equivalent to using
        net::io_context::post().
    */
    template<class ConstBufferSequence, class WriteHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(WriteHandler,
        void(boost::system::error_code, std::size_t))
    async_write_some(
        ConstBufferSequence const& buffers,
        WriteHandler&& handler);
};

//------------------------------------------------------------------------------

/// A TCP/IP socket wrapper which has a built-in asynchronous timeout
using timeout_socket = basic_timeout_socket<
    net::ip::tcp,
    net::io_context::executor_type>;

/**
    @defgroup async_connect boost::beast::async_connect

    @brief Asynchronously establishes a socket connection by trying each
        endpoint in a sequence, and terminating if a timeout occurs.
*/
/* @{ */
/** Asynchronously establishes a socket connection by trying each endpoint in a sequence.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the underlying socket's
    @c async_connect member function, once for each endpoint in the sequence,
    until a connection is successfully established or a timeout occurs.
    
    @param s The @ref basic_timeout_socket to be connected. If the socket
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
    timeout_socket s(ioc.get_executor());
    
    // ...

    r.async_resolve(q, resolve_handler);
    
    // ...
    
    void resolve_handler(
        boost::system::error_code const& ec,
        tcp::resolver::results_type results)
    {
        if (!ec)
        {
            async_connect(s, results, connect_handler);
        }
    }
    
    // ...

    void connect_handler(
        boost::system::error_code const& ec,
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
    void (boost::system::error_code, class Protocol::endpoint))
async_connect(
    basic_timeout_socket<Protocol, Executor>& s,
    EndpointSequence const& endpoints,
    RangeConnectHandler&& handler);

/** Asynchronously establishes a socket connection by trying each endpoint in a sequence.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the underlying socket's
    @c async_connect member function, once for each endpoint in the sequence,
    until a connection is successfully established or a timeout occurs.
    
    @param s The @ref basic_timeout_socket to be connected. If the socket
    is already open, it will be closed.
    
    @param endpoints A sequence of endpoints.
    
    @param connect_condition A function object that is called prior to each
    connection attempt. The signature of the function object must be:

    @code
    bool connect_condition(
        boost::system::error_code const& ec,
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
            boost::system::error_code const& ec,
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
    timeout_socket s(ioc.get_executor());
    
    // ...

    r.async_resolve(q, resolve_handler);
    
    // ...
    
    void resolve_handler(
        boost::system::error_code const& ec,
        tcp::resolver::results_type results)
    {
        if (!ec)
        {
            async_connect(s, results, my_connect_condition{}, connect_handler);
        }
    }
    
    // ...

    void connect_handler(
        boost::system::error_code const& ec,
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
    void (boost::system::error_code, class Protocol::endpoint))
async_connect(
    basic_timeout_socket<Protocol, Executor>& s,
    EndpointSequence const& endpoints,
    ConnectCondition connect_condition,
    RangeConnectHandler&& handler);

/** Asynchronously establishes a socket connection by trying each endpoint in a sequence.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the underlying socket's
    @c async_connect member function, once for each endpoint in the sequence,
    until a connection is successfully established or a timeout occurs.
    
    @param s The @ref timeout_socket to be connected. If the socket
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
    timeout_socket s(ioc.get_executor());
        
    async_connect(s,
        endpoints.begin(), endpoints.end(),
        connect_handler);

    void connect_handler(
        boost::system::error_code const& ec,
        std::vector<tcp::endpoint>::iterator)
    {
        // ...
    }
    @endcode
*/
template<
    class Protocol, class Executor,
    class Iterator,
    class IteratorConnectHandler
#if ! BOOST_BEAST_DOXYGEN
    ,class = typename std::enable_if<
        ! net::is_endpoint_sequence<
            Iterator>::value>::type
#endif
>
BOOST_ASIO_INITFN_RESULT_TYPE(IteratorConnectHandler,
    void (boost::system::error_code, Iterator))
async_connect(
    basic_timeout_socket<Protocol, Executor>& s,
    Iterator begin, Iterator end,
    IteratorConnectHandler&& handler);

/** Asynchronously establishes a socket connection by trying each endpoint in a sequence.
    
    This function attempts to connect a socket to one of a sequence of
    endpoints. It does this by repeated calls to the underlying socket's
    @c async_connect member function, once for each endpoint in the sequence,
    until a connection is successfully established or a timeout occurs.
    
    @param s The @ref basic_timeout_socket to be connected. If the socket
    is already open, it will be closed.
    
    @param begin An iterator pointing to the start of a sequence of endpoints.
    
    @param end An iterator pointing to the end of a sequence of endpoints.
    
    @param connect_condition A function object that is called prior to each
    connection attempt. The signature of the function object must be:

    @code
    bool connect_condition(
        boost::system::error_code const& ec,
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
            boost::system::error_code const& ec,
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
    timeout_socket s(ioc.get_executor());
        
    async_connect(s, endpoints.begin(), endpoints.end(),
        my_connect_condition{}, connect_handler);

    void connect_handler(
        boost::system::error_code const& ec,
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
    class IteratorConnectHandler
#if ! BOOST_BEAST_DOXYGEN
    ,class = typename std::enable_if<
        ! net::is_endpoint_sequence<
            Iterator>::value>::type
#endif
>
BOOST_ASIO_INITFN_RESULT_TYPE(IteratorConnectHandler,
    void (boost::system::error_code, Iterator))
async_connect(
    basic_timeout_socket<Protocol, Executor>& s,
    Iterator begin, Iterator end,
    ConnectCondition connect_condition,
    IteratorConnectHandler&& handler);
/* @} */

} // beast
} // boost

#include <boost/beast/_experimental/core/impl/timeout_socket.hpp>

#endif
