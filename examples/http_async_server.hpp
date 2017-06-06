//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_EXAMPLE_HTTP_ASYNC_SERVER_H_INCLUDED
#define BEAST_EXAMPLE_HTTP_ASYNC_SERVER_H_INCLUDED

#include "file_body.hpp"
#include "mime_type.hpp"

#include <beast/http.hpp>
#include <beast/core/handler_ptr.hpp>
#include <beast/core/multi_buffer.hpp>
#include <boost/asio.hpp>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

namespace beast {
namespace http {

class http_async_server
{
    using endpoint_type = boost::asio::ip::tcp::endpoint;
    using address_type = boost::asio::ip::address;
    using socket_type = boost::asio::ip::tcp::socket;

    using req_type = request<string_body>;
    using resp_type = response<file_body>;

    std::mutex m_;
    bool log_ = true;
    boost::asio::io_service ios_;
    boost::asio::ip::tcp::acceptor acceptor_;
    socket_type sock_;
    std::string root_;
    std::vector<std::thread> thread_;

public:
    http_async_server(endpoint_type const& ep,
            std::size_t threads, std::string const& root)
        : acceptor_(ios_)
        , sock_(ios_)
        , root_(root)
    {
        acceptor_.open(ep.protocol());
        acceptor_.bind(ep);
        acceptor_.listen(
            boost::asio::socket_base::max_connections);
        acceptor_.async_accept(sock_,
            std::bind(&http_async_server::on_accept, this,
                std::placeholders::_1));
        thread_.reserve(threads);
        for(std::size_t i = 0; i < threads; ++i)
            thread_.emplace_back(
                [&] { ios_.run(); });
    }

    ~http_async_server()
    {
        error_code ec;
        ios_.dispatch(
            [&]{ acceptor_.close(ec); });
        for(auto& t : thread_)
            t.join();
    }

    template<class... Args>
    void
    log(Args const&... args)
    {
        if(log_)
        {
            std::lock_guard<std::mutex> lock(m_);
            log_args(args...);
        }
    }

private:
    template<class Stream, class Handler,
        bool isRequest, class Body, class Fields>
    class write_op
    {
        struct data
        {
            bool cont;
            Stream& s;
            message<isRequest, Body, Fields> m;

            data(Handler& handler, Stream& s_,
                    message<isRequest, Body, Fields>&& m_)
                : s(s_)
                , m(std::move(m_))
            {
                using boost::asio::asio_handler_is_continuation;
                cont = asio_handler_is_continuation(std::addressof(handler));
            }
        };

        handler_ptr<data, Handler> d_;

    public:
        write_op(write_op&&) = default;
        write_op(write_op const&) = default;

        template<class DeducedHandler, class... Args>
        write_op(DeducedHandler&& h, Stream& s, Args&&... args)
            : d_(std::forward<DeducedHandler>(h),
                s, std::forward<Args>(args)...)
        {
            (*this)(error_code{}, false);
        }

        void
        operator()(error_code ec, bool again = true)
        {
            auto& d = *d_;
            d.cont = d.cont || again;
            if(! again)
            {
                beast::http::async_write(d.s, d.m, std::move(*this));
                return;
            }
            d_.invoke(ec);
        }

        friend
        void* asio_handler_allocate(
            std::size_t size, write_op* op)
        {
            using boost::asio::asio_handler_allocate;
            return asio_handler_allocate(
                size, std::addressof(op->d_.handler()));
        }

        friend
        void asio_handler_deallocate(
            void* p, std::size_t size, write_op* op)
        {
            using boost::asio::asio_handler_deallocate;
            asio_handler_deallocate(
                p, size, std::addressof(op->d_.handler()));
        }

        friend
        bool asio_handler_is_continuation(write_op* op)
        {
            return op->d_->cont;
        }

        template<class Function>
        friend
        void asio_handler_invoke(Function&& f, write_op* op)
        {
            using boost::asio::asio_handler_invoke;
            asio_handler_invoke(
                f, std::addressof(op->d_.handler()));
        }
    };

    template<class Stream,
        bool isRequest, class Body, class Fields,
            class DeducedHandler>
    static
    void
    async_write(Stream& stream, message<
        isRequest, Body, Fields>&& msg,
            DeducedHandler&& handler)
    {
        write_op<Stream, typename std::decay<DeducedHandler>::type,
            isRequest, Body, Fields>{std::forward<DeducedHandler>(
                handler), stream, std::move(msg)};
    }

    class peer : public std::enable_shared_from_this<peer>
    {
        int id_;
        multi_buffer sb_;
        socket_type sock_;
        http_async_server& server_;
        boost::asio::io_service::strand strand_;
        req_type req_;

    public:
        peer(peer&&) = default;
        peer(peer const&) = default;
        peer& operator=(peer&&) = delete;
        peer& operator=(peer const&) = delete;

        peer(socket_type&& sock, http_async_server& server)
            : sock_(std::move(sock))
            , server_(server)
            , strand_(sock_.get_io_service())
        {
            static int n = 0;
            id_ = ++n;
        }

        void
        fail(error_code ec, std::string what)
        {
            if(ec != boost::asio::error::operation_aborted)
                server_.log("#", id_, " ", what, ": ", ec.message(), "\n");
        }

        void run()
        {
            do_read();
        }

        void do_read()
        {
            async_read(sock_, sb_, req_, strand_.wrap(
                std::bind(&peer::on_read, shared_from_this(),
                    std::placeholders::_1)));
        }

        void on_read(error_code const& ec)
        {
            if(ec)
                return fail(ec, "read");
            auto path = req_.target().to_string();
            if(path == "/")
                path = "/index.html";
            path = server_.root_ + path;
            if(! boost::filesystem::exists(path))
            {
                response<string_body> res;
                res.result(status::not_found);
                res.version = req_.version;
                res.insert("Server", "http_async_server");
                res.insert("Content-Type", "text/html");
                res.body = "The file '" + path + "' was not found";
                res.prepare();
                async_write(sock_, std::move(res),
                    std::bind(&peer::on_write, shared_from_this(),
                        std::placeholders::_1));
                return;
            }
            try
            {
                resp_type res;
                res.result(status::ok);
                res.version = req_.version;
                res.insert("Server", "http_async_server");
                res.insert("Content-Type", mime_type(path));
                res.body = path;
                res.prepare();
                async_write(sock_, std::move(res),
                    std::bind(&peer::on_write, shared_from_this(),
                        std::placeholders::_1));
            }
            catch(std::exception const& e)
            {
                response<string_body> res;
                res.result(status::internal_server_error);
                res.version = req_.version;
                res.insert("Server", "http_async_server");
                res.insert("Content-Type", "text/html");
                res.body =
                    std::string{"An internal error occurred"} + e.what();
                res.prepare();
                async_write(sock_, std::move(res),
                    std::bind(&peer::on_write, shared_from_this(),
                        std::placeholders::_1));
            }
        }

        void on_write(error_code ec)
        {
            if(ec)
                fail(ec, "write");
            do_read();
        }
    };

    void
    log_args()
    {
    }

    template<class Arg, class... Args>
    void
    log_args(Arg const& arg, Args const&... args)
    {
        std::cerr << arg;
        log_args(args...);
    }

    void
    fail(error_code ec, std::string what)
    {
        log(what, ": ", ec.message(), "\n");
    }

    void
    on_accept(error_code ec)
    {
        if(! acceptor_.is_open())
            return;
        if(ec)
            return fail(ec, "accept");
        socket_type sock(std::move(sock_));
        acceptor_.async_accept(sock_,
            std::bind(&http_async_server::on_accept, this,
                std::placeholders::_1));
        std::make_shared<peer>(std::move(sock), *this)->run();
    }
};

} // http
} // beast

#endif
