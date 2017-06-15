//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef WEBSOCKET_SYNC_ECHO_SERVER_HPP
#define WEBSOCKET_SYNC_ECHO_SERVER_HPP

#include <beast/core/multi_buffer.hpp>
#include <beast/websocket.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

namespace websocket {

/** Synchronous WebSocket echo client/server
*/
class sync_echo_server
{
public:
    using error_code = beast::error_code;
    using endpoint_type = boost::asio::ip::tcp::endpoint;
    using address_type = boost::asio::ip::address;
    using socket_type = boost::asio::ip::tcp::socket;

private:
    std::ostream* log_;
    boost::asio::io_service ios_;
    socket_type sock_;
    endpoint_type ep_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::thread thread_;
    std::function<void(beast::websocket::stream<socket_type>&)> mod_;

public:
    /** Constructor.

        @param log A pointer to a stream to log to, or `nullptr`
        to disable logging.
    */
    sync_echo_server(std::ostream* log)
        : log_(log)
        , sock_(ios_)
        , acceptor_(ios_)
    {
    }

    /** Destructor.
    */
    ~sync_echo_server()
    {
        if(thread_.joinable())
        {
            error_code ec;
            ios_.dispatch(
                [&]{ acceptor_.close(ec); });
            thread_.join();
        }
    }

    /** Return the listening endpoint.
    */
    endpoint_type
    local_endpoint() const
    {
        return acceptor_.local_endpoint();
    }

    /** Set a handler called for new streams.

        This function is called for each new stream.
        It is used to set options for every connection.
    */
    template<class F>
    void
    on_new_stream(F const& f)
    {
        mod_ = f;
    }

    /** Open a listening port.

        @param ep The address and port to bind to.

        @param ec Set to the error, if any occurred.
    */
    void
    open(endpoint_type const& ep, error_code& ec)
    {
        acceptor_.open(ep.protocol(), ec);
        if(ec)
            return fail("open", ec);
        acceptor_.set_option(
            boost::asio::socket_base::reuse_address{true});
        acceptor_.bind(ep, ec);
        if(ec)
            return fail("bind", ec);
        acceptor_.listen(
            boost::asio::socket_base::max_connections, ec);
        if(ec)
            return fail("listen", ec);
        acceptor_.async_accept(sock_, ep_,
            std::bind(&sync_echo_server::on_accept, this,
                std::placeholders::_1));
        thread_ = std::thread{[&]{ ios_.run(); }};
    }

private:
    void
    fail(std::string what, error_code ec)
    {
        if(log_)
        {
            static std::mutex m;
            std::lock_guard<std::mutex> lock{m};
            (*log_) << what << ": " <<
                ec.message() << std::endl;
        }
    }

    void
    fail(std::string what, error_code ec,
        int id, endpoint_type const& ep)
    {
        if(log_)
            if(ec != beast::websocket::error::closed)
                fail("[#" + std::to_string(id) + " " +
                    boost::lexical_cast<std::string>(ep) +
                        "] " + what, ec);
    }

    void
    on_accept(error_code ec)
    {
        if(ec == boost::asio::error::operation_aborted)
            return;
        if(ec)
            return fail("accept", ec);
        struct lambda
        {
            std::size_t id;
            endpoint_type ep;
            sync_echo_server& self;
            boost::asio::io_service::work work;
            // Must be destroyed before work otherwise the
            // io_service could be destroyed before the socket.
            socket_type sock;

            lambda(sync_echo_server& self_,
                endpoint_type const& ep_,
                    socket_type&& sock_)
                : id([]
                    {
                        static std::atomic<std::size_t> n{0};
                        return ++n;
                    }())
                , ep(ep_)
                , self(self_)
                , work(sock_.get_io_service())
                , sock(std::move(sock_))
            {
            }

            void operator()()
            {
                self.do_peer(id, ep, std::move(sock));
            }
        };
        std::thread{lambda{*this, ep_, std::move(sock_)}}.detach();
        acceptor_.async_accept(sock_, ep_,
            std::bind(&sync_echo_server::on_accept, this,
                std::placeholders::_1));
    }

    void
    do_peer(std::size_t id,
        endpoint_type const& ep, socket_type&& sock)
    {
        using boost::asio::buffer;
        using boost::asio::buffer_copy;
        beast::websocket::stream<
            socket_type> ws{std::move(sock)};
        mod_(ws);
        error_code ec;
        ws.accept_ex(
            [](beast::websocket::response_type& res)
            {
                res.insert(
                    "Server", "sync_echo_server");
            },
            ec);
        if(ec)
        {
            fail("accept", ec, id, ep);
            return;
        }
        for(;;)
        {
            beast::multi_buffer b;
            ws.read(b, ec);
            if(ec)
            {
                auto const s = ec.message();
                break;
            }
            ws.binary(ws.got_binary());
            ws.write(b.data(), ec);
            if(ec)
                break;
        }
        if(ec && ec != beast::websocket::error::closed)
        {
            fail("read", ec, id, ep);
        }
    }
};

} // websocket

#endif
