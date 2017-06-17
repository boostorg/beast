//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_EXAMPLE_SERVER_HTTP_SYNC_PORT_HPP
#define BEAST_EXAMPLE_SERVER_HTTP_SYNC_PORT_HPP

#include "server.hpp"

#include "http_base.hpp"
#include "rfc7231.hpp"
#include "service_list.hpp"
#include "write_msg.hpp"

#include <beast/core/flat_buffer.hpp>
#include <beast/core/handler_ptr.hpp>
#include <beast/http/dynamic_body.hpp>
#include <beast/http/parser.hpp>
#include <beast/http/read.hpp>
#include <beast/http/string_body.hpp>
#include <beast/http/write.hpp>
#include <memory>
#include <utility>
#include <ostream>
#include <thread>

namespace framework {

/** A synchronous HTTP connection.

    This base class implements an HTTP connection object using
    synchronous calls.

    It uses the Curiously Recurring Template pattern (CRTP) where
    we refer to the derived class in order to access the stream object
    to use for reading and writing. This lets the same class be used
    for plain and SSL stream objects.

    @tparam Services The list of services this connection will support.
*/
template<class Derived, class... Services>
class sync_http_con
    : public http_base
{
    // This function lets us access members of the derived class
    Derived&
    impl()
    {
        return static_cast<Derived&>(*this);
    }

    // The stream to use for logging
    std::ostream& log_;

    // The services configured for the port
    service_list<Services...> const& services_;

    // A small unique integer for logging
    std::size_t id_;

    // The remote endpoint. We cache it here because
    // calls to remote_endpoint() can fail / throw.
    //
    endpoint_type ep_;

    // The buffer for performing reads
    beast::flat_buffer buffer_;

public:
    /// Constructor
    sync_http_con(
        beast::string_view server_name,
        std::ostream& log,
        service_list<Services...> const& services,
        std::size_t id,
        endpoint_type const& ep)
        : http_base(server_name)
        , log_(log)
        , services_(services)
        , id_(id)
        , ep_(ep)

        // The buffer has a limit of 8192, otherwise
        // the server is vulnerable to a buffer attack.
        //
        , buffer_(8192)
    {
    }

    void
    run()
    {
        // Bind a shared pointer into the lambda for the
        // thread, so the sync_http_con is destroyed after
        // the thread function exits.
        //
        std::thread{
            &sync_http_con::do_run,
            impl().shared_from_this()
        }.detach();
    }

private:
    // This lambda is passed to the service list to handle
    // the case of sending request objects of varying types.
    // In C++14 this is more easily accomplished using a generic
    // lambda, but we want C+11 compatibility so we manually
    // write the lambda out.
    //
    struct send_lambda
    {
        // holds "this"
        sync_http_con& self_;

        // holds the captured error code
        error_code& ec_;

    public:
        // Constructor
        //
        // Capture "this" and "ec"
        //
        send_lambda(sync_http_con& self, error_code& ec)
            : self_(self)
            , ec_(ec)
        {
        }

        // Sends a message
        //
        // Since this is a synchronous implementation we
        // just call the write function and block.
        //
        template<class Body, class Fields>
        void
        operator()(
            beast::http::response<Body, Fields>&& res) const
        {
            beast::http::write(self_.impl().stream(), res, ec_);
        }
    };

    void
    do_run()
    {
        // The main connection loop, we alternate between
        // reading a request and sending a response. On
        // error we log and return, which destroys the thread
        // and the stream (thus closing the connection)
        //
        for(;;)
        {
            error_code ec;

            // Arguments passed to the parser constructor are
            // forwarded to the message object. A single argument
            // is forwarded to the body constructor.
            //
            // We construct the dynamic body with a 1MB limit
            // to prevent vulnerability to buffer attacks.
            //
            beast::http::request_parser<beast::http::dynamic_body> parser(1024* 1024);

            // Read the header first
            beast::http::read_header(impl().stream(), buffer_, parser, ec);

            if(ec)
                return fail("on_read", ec);

            send_lambda send{*this, ec};

            auto& req = parser.get();

            // See if they are specifying Expect: 100-continue
            //
            if(rfc7231::is_expect_100_continue(req))
            {
                // They want to know if they should continue,
                // so send the appropriate response synchronously.
                //
                send(this->continue_100(req));
            }

            // Read the rest of the message, if any.
            //
            beast::http::read(impl().stream(), buffer_, parser, ec);

            // Give each services a chance to handle the request
            //
            if(! services_.respond(
                    impl().stream(),
                    ep_,
                    std::move(req),
                    send))
            {
                // No service handled the request,
                // send a Bad Request result to the client.
                //
                send(this->bad_request(req));
            }
            else
            {
                // See if the service that handled the
                // response took ownership of the stream.
                if(! impl().stream().is_open())
                {
                    // They took ownership so just return and
                    // let this sync_http_con object get destroyed.
                    return;
                }
            }

            if(ec)
                return fail("on_write", ec);

            // Theres no pipelining possible in a synchronous server
            // because we can't do reads and writes at the same time.
        }
    }

    // Called when a failure occurs
    //
    void
    fail(std::string what, error_code ec)
    {
        if( ec != beast::http::error::end_of_stream &&
            ec != boost::asio::error::operation_aborted)
        {
            log_ <<
                "[#" << id_ << " " << ep_ << "] " <<
            what << ": " << ec.message() << std::endl;
        }
    }
};

//------------------------------------------------------------------------------

// This class represents a synchronous HTTP connection which
// uses a plain TCP/IP socket (no encryption) as the stream.
//
template<class... Services>
class sync_http_con_plain

    // Note that we give this object the `enable_shared_from_this`, and have
    // the base class call `impl().shared_from_this()` when needed.
    //
    : public std::enable_shared_from_this<sync_http_con_plain<Services...>>

    // We want the socket to be created before the base class so we use
    // the "base from member" idiom which Boost provides as a class.
    //
    , public base_from_member<socket_type>

    // Declare this base last now that everything else got set up first.
    //
    , public sync_http_con<sync_http_con_plain<Services...>, Services...>
{
public:
    // Construct the plain connection.
    //
    template<class... Args>
    sync_http_con_plain(
        socket_type&& sock,
        Args&&... args)
        : base_from_member<socket_type>(std::move(sock))
        , sync_http_con<sync_http_con_plain<Services...>, Services...>(
            std::forward<Args>(args)...)
    {
    }

    // Returns the stream.
    //
    // The base class calls this to obtain the object to
    // use for reading and writing HTTP messages.
    //
    socket_type&
    stream()
    {
        return this->member;
    }
};

//------------------------------------------------------------------------------

/*  A synchronous HTTP port handler

    This type meets the requirements of @b PortHandler. It supports
    variable list of HTTP services in its template parameter list,
    and provides a synchronous connection implementation to service
*/
template<class... Services>
class http_sync_port
{
    server& instance_;
    std::ostream& log_;
    service_list<Services...> services_;

public:
    /** Constructor

        @param instance The server instance which owns this port

        @param log The stream to use for logging
    */
    http_sync_port(
        server& instance,
        std::ostream& log)
        : instance_(instance)
        , log_(log)
    {
    }

    /** Initialize a service

        Every service in the list must be initialized exactly once.

        @param ec Set to the error, if any occurred

        @param args Optional arguments forwarded to the service
        constructor.

        @tparam Index The 0-based index of the service to initialize.
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
        std::make_shared<sync_http_con_plain<Services...>>(
            std::move(sock),
            "http_sync_port",
            log_,
            services_,
            instance_.next_id(),
            ep
                )->run();
    }
};

} // framework

#endif
