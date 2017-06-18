//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_EXAMPLE_SERVER_MULTI_PORT_HPP
#define BEAST_EXAMPLE_SERVER_MULTI_PORT_HPP

#include "detect_ssl.hpp"
#include "ws_async_port.hpp"
#include "http_async_port.hpp"
#include "https_ports.hpp"
#include "wss_ports.hpp"

#include <beast/core.hpp>

#include <boost/function.hpp>

namespace framework {

// This class represents a connection that detects an opening SSL handshake.
//
// If the handshake is detected, an HTTPS connection object is move
// constructed from this object. Otherwise, this object continues as
// a normal unencrypted HTTP connection. If the underlying port has
// the ws_upgrade_service configured, the connection may be optionally
// be upgraded to WebSocket by the client.
//
template<class... Services>
class multi_con

    // Note that we give this object the enable_shared_from_this, and have
    // the base class call impl().shared_from_this(). The reason we do that
    // is so that the shared_ptr has the correct type. This lets the
    // derived class (this class) use its members in calls to std::bind,
    // without an ugly call to `dynamic_downcast` or other nonsense.
    //
    : public std::enable_shared_from_this<multi_con<Services...>>

    // We want the socket to be created before the
    // base class so we use the "base from member" idiom.
    //
    , public base_from_member<socket_type>

    // Declare this base last now that everything else got set up first.
    //
    , public async_http_con_base<multi_con<Services...>, Services...>
{
    // This is the context to use if we get an SSL handshake
    boost::asio::ssl::context& ctx_;

    // This buffer holds the data we read during ssl detection
    beast::static_buffer_n<6> buffer_;

public:
    // Construct the plain connection.
    //
    template<class... Args>
    multi_con(
        socket_type&& sock,
        boost::asio::ssl::context& ctx,
        Args&&... args)
        : base_from_member<socket_type>(std::move(sock))
        , async_http_con_base<multi_con<Services...>, Services...>(
            std::forward<Args>(args)...)
        , ctx_(ctx)
    {
    }

    // Returns the stream.
    // The base class calls this to obtain the object to
    // use for reading and writing HTTP messages.
    //
    socket_type&
    stream()
    {
        return this->member;
    }

    // Called by the port to launch the connection in detect mode
    void
    detect()
    {
        async_detect_ssl(stream(), buffer_, this->strand_.wrap(
            std::bind(&multi_con::on_detect, this->shared_from_this(),
                std::placeholders::_1, std::placeholders::_2)));
    }

private:
    // Base class needs to be a friend to call our private members
    friend class async_http_con_base<multi_con<Services...>, Services...>;

    void
    on_detect(error_code ec, boost::tribool result)
    {
        // Check for a failure
        if(ec)
            return this->fail("on_detect", ec);

        // See if we detected SSL
        if(result)
        {
            // Get the remote endpoint, we need
            // it to construct the new connection.
            //
            endpoint_type ep = stream().remote_endpoint(ec);
            if(ec)
                return this->fail("remote_endpoint", ec);

            // Yes, so launch an async HTTPS connection
            //
            std::make_shared<async_https_con<Services...>>(
                std::move(stream()),
                ctx_,
                "multi_port",
                this->log_,
                this->services_,
                this->id_,
                ep
                    )->handshake(buffer_.data());
            return;
        }

        // No, so start the HTTP connection normally.
        // Since we read some bytes from the connection that might
        // contain an HTTP request, we pass the buffer holding those
        // bytes to the base class so it can use them.
        //
        this->run(buffer_.data());
    }

    // This is called by the base before running the main loop.
    //
    void
    do_handshake()
    {
        // Run the main loop right away
        //
        this->do_run();
    }

    // This is called when the other end closes the connection gracefully.
    //
    void
    do_shutdown()
    {
        error_code ec;
        stream().shutdown(socket_type::shutdown_both, ec);
        if(ec)
            return this->fail("shutdown", ec);
    }
};

//------------------------------------------------------------------------------

/*  An asynchronous HTTP and WebSocket port handler, plain or SSL

    This type meets the requirements of @b PortHandler. It supports
    variable list of HTTP services in its template parameter list,
    and provides a synchronous connection implementation to service.

    The port will automatically detect OpenSSL handshakes and establish
    encrypted connections, otherwise will use a plain unencrypted
    connection. This all happens through the same port.
*/
class multi_port_base
{
protected:
    // VFALCO We use boost::function to work around a compiler
    //        crash with gcc and clang using libstdc++

    // The types of the on_stream callback
    using on_new_stream_cb1 = boost::function<
        void(beast::websocket::stream<socket_type>&)>;
    using on_new_stream_cb2 = boost::function<
        void(beast::websocket::stream<ssl_stream<socket_type>>&)>;

    // Reference to the server instance that made us
    server& instance_;

    // The stream to log to
    std::ostream& log_;

    // The context holds the SSL certificates the server uses
    boost::asio::ssl::context& ctx_;

    // Called for each new websocket stream
    on_new_stream_cb1 cb1_;
    on_new_stream_cb2 cb2_;

public:
    /** Constructor

        @param instance The server instance which owns this port

        @param log The stream to use for logging

        @param ctx The SSL context holding the SSL certificates to use

        @param cb A callback which will be invoked for every new
        WebSocket connection. This provides an opportunity to change
        the settings on the stream before it is used. The callback
        should have this equivalent signature:
        @code
        template<class NextLayer>
        void callback(beast::websocket::stream<NextLayer>&);
        @endcode
        In C++14 this can be accomplished with a generic lambda. In
        C++11 it will be necessary to write out a lambda manually,
        with a templated operator().
    */
    template<class Callback>
    multi_port_base(
        server& instance,
        std::ostream& log,
        boost::asio::ssl::context& ctx,
        Callback const& cb)
        : instance_(instance)
        , log_(log)
        , ctx_(ctx)
        , cb1_(cb)
        , cb2_(cb)
    {
    }

    /** Accept a WebSocket upgrade request.

        This is used to accept a connection that has already
        delivered the handshake.

        @param stream The stream corresponding to the connection.

        @param ep The remote endpoint.

        @param req The upgrade request.
    */
    template<class Body, class Fields>
    void
    on_upgrade(
        socket_type&& sock,
        endpoint_type ep,
        beast::http::request<Body, Fields>&& req)
    {
        std::make_shared<async_ws_con>(
            std::move(sock),
            "multi_port",
            log_,
            instance_.next_id(),
            ep,
            cb1_
                )->run(std::move(req));
    }

    /** Accept a WebSocket upgrade request.

        This is used to accept a connection that has already
        delivered the handshake.

        @param stream The stream corresponding to the connection.

        @param ep The remote endpoint.

        @param req The upgrade request.
    */
    template<class Body, class Fields>
    void
    on_upgrade(
        ssl_stream<socket_type>&& stream,
        endpoint_type ep,
        beast::http::request<Body, Fields>&& req)
    {
        std::make_shared<async_wss_con>(
            std::move(stream),
            "multi_port",
            log_,
            instance_.next_id(),
            ep,
            cb2_
                )->run(std::move(req));
    }
};

template<class... Services>
class multi_port : public multi_port_base
{
    // The list of services connections created from this port will support
    service_list<Services...> services_;

public:
    /** Constructor

        All arguments are forwarded to the multi_port_base constructor.
    */
    template<class... Args>
    multi_port(Args&&... args)
        : multi_port_base(std::forward<Args>(args)...)
    {
    }

    /** Initialize a service

        Every service in the list must be initialized exactly once.

        @param args Optional arguments forwarded to the service
        constructor.

        @tparam Index The 0-based index of the service to initialize.

        @return A reference to the service list. This permits
        calls to be chained in a single expression.
    */
    template<std::size_t Index, class... Args>
    void
    init(error_code& ec, Args&&... args)
    {
        services_.template init<Index>(
            ec,
            std::forward<Args>(args)...);
    }

    /** Called by the server to provide ownership of the socket for a new connection

        @param sock The socket whose ownership is to be transferred

        @ep The remote endpoint
    */
    void
    on_accept(socket_type&& sock, endpoint_type ep)
    {
        // Create a plain http connection object
        // and transfer ownership of the socket.
        //
        std::make_shared<multi_con<Services...>>(
            std::move(sock),
            ctx_,
            "multi_port",
            log_,
            services_,
            instance_.next_id(),
            ep
                )->detect();
    }
};

} // framework

#endif
