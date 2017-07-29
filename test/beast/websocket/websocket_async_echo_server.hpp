//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_ASYNC_ECHO_SERVER_HPP
#define BOOST_BEAST_WEBSOCKET_ASYNC_ECHO_SERVER_HPP

#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <thread>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace websocket {

/** Asynchronous WebSocket echo client/server
*/
class async_echo_server
{
public:
    using error_code = boost::beast::error_code;
    using address_type = boost::asio::ip::address;
    using socket_type = boost::asio::ip::tcp::socket;
    using endpoint_type = boost::asio::ip::tcp::endpoint;

private:
    /** A container of type-erased option setters.
    */
    template<class NextLayer>
    class options_set
    {
        // workaround for std::function bug in msvc
        struct callable
        {
            virtual ~callable() = default;
            virtual void operator()(
                boost::beast::websocket::stream<NextLayer>&) = 0;
        };

        template<class T>
        class callable_impl : public callable
        {
            T t_;

        public:
            template<class U>
            callable_impl(U&& u)
                : t_(std::forward<U>(u))
            {
            }

            void
            operator()(boost::beast::websocket::stream<NextLayer>& ws)
            {
                t_(ws);
            }
        };

        template<class Opt>
        class lambda
        {
            Opt opt_;

        public:
            lambda(lambda&&) = default;
            lambda(lambda const&) = default;

            lambda(Opt const& opt)
                : opt_(opt)
            {
            }

            void
            operator()(boost::beast::websocket::stream<NextLayer>& ws) const
            {
                ws.set_option(opt_);
            }
        };

        std::unordered_map<std::type_index,
            std::unique_ptr<callable>> list_;

    public:
        template<class Opt>
        void
        set_option(Opt const& opt)
        {
            std::unique_ptr<callable> p;
            p.reset(new callable_impl<lambda<Opt>>{opt});
            list_[std::type_index{
                typeid(Opt)}] = std::move(p);
        }

        void
        set_options(boost::beast::websocket::stream<NextLayer>& ws)
        {
            for(auto const& op : list_)
                (*op.second)(ws);
        }
    };

    std::ostream* log_;
    boost::asio::io_service ios_;
    socket_type sock_;
    endpoint_type ep_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::vector<std::thread> thread_;
    boost::optional<boost::asio::io_service::work> work_;
    options_set<socket_type> opts_;

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
        error_code ec;
        ios_.dispatch(
            [&]{ acceptor_.close(ec); });
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

    /** Set a websocket option.

        The option will be applied to all new connections.

        @param opt The option to apply.
    */
    template<class Opt>
    void
    set_option(Opt const& opt)
    {
        opts_.set_option(opt);
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
            boost::beast::websocket::stream<socket_type> ws;
            boost::asio::io_service::strand strand;
            boost::beast::multi_buffer db;
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
            d.server.opts_.set_options(d.ws);
            run();
        }

        void run()
        {
            auto& d = *d_;
            d.ws.async_accept_ex(
                [](boost::beast::websocket::response_type& res)
                {
                    res.insert(
                        "Server", "async_echo_server");
                },
                std::move(*this));
        }

        template<class DynamicBuffer, std::size_t N>
        static
        bool
        match(DynamicBuffer& db, char const(&s)[N])
        {
            using boost::asio::buffer;
            using boost::asio::buffer_copy;
            if(db.size() < N-1)
                return false;
            boost::beast::static_string<N-1> t;
            t.resize(N-1);
            buffer_copy(buffer(t.data(), t.size()),
                db.data());
            if(t != s)
                return false;
            db.consume(N-1);
            return true;
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
                if(ec == boost::beast::websocket::error::closed)
                    return;
                if(ec)
                    return fail("async_read", ec);
                if(match(d.db, "RAW"))
                {
                    d.state = 1;
                    boost::asio::async_write(d.ws.next_layer(),
                        d.db.data(), d.strand.wrap(std::move(*this)));
                    return;
                }
                else if(match(d.db, "TEXT"))
                {
                    d.state = 1;
                    d.ws.binary(false);
                    d.ws.async_write(
                        d.db.data(), d.strand.wrap(std::move(*this)));
                    return;
                }
                else if(match(d.db, "PING"))
                {
                    boost::beast::websocket::ping_data payload;
                    d.db.consume(buffer_copy(
                        buffer(payload.data(), payload.size()),
                            d.db.data()));
                    d.state = 1;
                    d.ws.async_ping(payload,
                        d.strand.wrap(std::move(*this)));
                    return;
                }
                else if(match(d.db, "CLOSE"))
                {
                    d.state = 1;
                    d.ws.async_close({},
                        d.strand.wrap(std::move(*this)));
                    return;
                }
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
                if(ec != boost::beast::websocket::error::closed)
                    d.server.fail("[#" + std::to_string(d.id) +
                        " " + d.ep.address().to_string() + ":" +
                            std::to_string(d.ep.port()) + "] " + what, ec);
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
