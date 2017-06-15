//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef WEBSOCKET_ASYNC_ECHO_SERVER_HPP
#define WEBSOCKET_ASYNC_ECHO_SERVER_HPP

#include <beast/core/multi_buffer.hpp>
#include <beast/websocket/stream.hpp>
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

/** Asynchronous WebSocket echo client/server
*/
class async_echo_server
{
public:
    using error_code = beast::error_code;
    using address_type = boost::asio::ip::address;
    using socket_type = boost::asio::ip::tcp::socket;
    using endpoint_type = boost::asio::ip::tcp::endpoint;

private:
    std::ostream* log_;
    boost::asio::io_service ios_;
    socket_type sock_;
    endpoint_type ep_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::vector<std::thread> thread_;
    boost::optional<boost::asio::io_service::work> work_;
    std::function<void(beast::websocket::stream<socket_type>&)> mod_;

public:
    async_echo_server(async_echo_server const&) = delete;
    async_echo_server& operator=(async_echo_server const&) = delete;

    /** Constructor.

        @param log A pointer to a stream to log to, or `nullptr`
        to disable logging.

        @param threads The number of threads in the io_service.
    */
    async_echo_server(std::ostream* log,
            std::size_t threads)
        : log_(log)
        , sock_(ios_)
        , acceptor_(ios_)
        , work_(ios_)
    {
        thread_.reserve(threads);
        for(std::size_t i = 0; i < threads; ++i)
            thread_.emplace_back(
                [&]{ ios_.run(); });
    }

    /** Destructor.
    */
    ~async_echo_server()
    {
        work_ = boost::none;
        ios_.dispatch(
            [&]
            {
                error_code ec;
                acceptor_.close(ec);
            });
        for(auto& t : thread_)
            t.join();
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
            std::bind(&async_echo_server::on_accept, this,
                std::placeholders::_1));
    }

private:
    class peer
    {
        struct data
        {
            async_echo_server& server;
            endpoint_type ep;
            int state = 0;
            beast::websocket::stream<socket_type> ws;
            boost::asio::io_service::strand strand;
            beast::multi_buffer db;
            std::size_t id;

            data(async_echo_server& server_,
                endpoint_type const& ep_,
                    socket_type&& sock_)
                : server(server_)
                , ep(ep_)
                , ws(std::move(sock_))
                , strand(ws.get_io_service())
                , id([]
                    {
                        static std::atomic<std::size_t> n{0};
                        return ++n;
                    }())
            {
            }
        };

        // VFALCO This could be unique_ptr in [Net.TS]
        std::shared_ptr<data> d_;

    public:
        peer(peer&&) = default;
        peer(peer const&) = default;
        peer& operator=(peer&&) = delete;
        peer& operator=(peer const&) = delete;

        template<class... Args>
        explicit
        peer(async_echo_server& server,
            endpoint_type const& ep, socket_type&& sock,
                Args&&... args)
            : d_(std::make_shared<data>(server, ep,
                std::forward<socket_type>(sock),
                    std::forward<Args>(args)...))
        {
            auto& d = *d_;
            d.server.mod_(d.ws);
            run();
        }

        void run()
        {
            auto& d = *d_;
            d.ws.async_accept_ex(
                [](beast::websocket::response_type& res)
                {
                    res.insert(
                        "Server", "async_echo_server");
                },
                std::move(*this));
        }

        void operator()(error_code ec, std::size_t)
        {
            (*this)(ec);
        }

        void operator()(error_code ec)
        {
            using boost::asio::buffer;
            using boost::asio::buffer_copy;
            auto& d = *d_;
            switch(d.state)
            {
            // did accept
            case 0:
                if(ec)
                    return fail("async_accept", ec);

            // start
            case 1:
                if(ec)
                    return fail("async_handshake", ec);
                d.db.consume(d.db.size());
                // read message
                d.state = 2;
                d.ws.async_read(d.db,
                    d.strand.wrap(std::move(*this)));
                return;

            // got message
            case 2:
                if(ec == beast::websocket::error::closed)
                    return;
                if(ec)
                    return fail("async_read", ec);
                // write message
                d.state = 1;
                d.ws.binary(d.ws.got_binary());
                d.ws.async_write(d.db.data(),
                    d.strand.wrap(std::move(*this)));
                return;
            }
        }

    private:
        void
        fail(std::string what, error_code ec)
        {
            auto& d = *d_;
            if(d.server.log_)
                if(ec != beast::websocket::error::closed)
                    d.server.fail("[#" + std::to_string(d.id) +
                        " " + boost::lexical_cast<std::string>(d.ep) +
                            "] " + what, ec);
        }
    };

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
    on_accept(error_code ec)
    {
        if(! acceptor_.is_open())
            return;
        if(ec == boost::asio::error::operation_aborted)
            return;
        if(ec)
            fail("accept", ec);
        peer{*this, ep_, std::move(sock_)};
        acceptor_.async_accept(sock_, ep_,
            std::bind(&async_echo_server::on_accept, this,
                std::placeholders::_1));
    }
};

} // websocket

#endif
