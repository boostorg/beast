//
// Copyright (c) 2022 Klemens D. Morgenstern (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: Advanced server, flex (plain + SSL)
//
//------------------------------------------------------------------------------

#include "example/common/server_certificate.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/make_unique.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/optional.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)


namespace beast = boost::beast;                 // from <boost/beast.hpp>
namespace http = beast::http;                   // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;         // from <boost/beast/websocket.hpp>
namespace net = boost::asio;                    // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;               // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;               // from <boost/asio/ip/tcp.hpp>

using executor_type = net::io_context::executor_type;
using executor_with_default = net::as_tuple_t<net::use_awaitable_t<executor_type>>::executor_with_default<executor_type>;



// Return a reasonable mime type based on the extension of a file.
beast::string_view
mime_type(beast::string_view path)
{
    using beast::iequals;
    auto const ext = [&path]
    {
        auto const pos = path.rfind(".");
        if(pos == beast::string_view::npos)
            return beast::string_view{};
        return path.substr(pos);
    }();
    if(iequals(ext, ".htm"))  return "text/html";
    if(iequals(ext, ".html")) return "text/html";
    if(iequals(ext, ".php"))  return "text/html";
    if(iequals(ext, ".css"))  return "text/css";
    if(iequals(ext, ".txt"))  return "text/plain";
    if(iequals(ext, ".js"))   return "application/javascript";
    if(iequals(ext, ".json")) return "application/json";
    if(iequals(ext, ".xml"))  return "application/xml";
    if(iequals(ext, ".swf"))  return "application/x-shockwave-flash";
    if(iequals(ext, ".flv"))  return "video/x-flv";
    if(iequals(ext, ".png"))  return "image/png";
    if(iequals(ext, ".jpe"))  return "image/jpeg";
    if(iequals(ext, ".jpeg")) return "image/jpeg";
    if(iequals(ext, ".jpg"))  return "image/jpeg";
    if(iequals(ext, ".gif"))  return "image/gif";
    if(iequals(ext, ".bmp"))  return "image/bmp";
    if(iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
    if(iequals(ext, ".tiff")) return "image/tiff";
    if(iequals(ext, ".tif"))  return "image/tiff";
    if(iequals(ext, ".svg"))  return "image/svg+xml";
    if(iequals(ext, ".svgz")) return "image/svg+xml";
    return "application/text";
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
std::string
path_cat(
    beast::string_view base,
    beast::string_view path)
{
    if(base.empty())
        return std::string(path);
    std::string result(base);
#ifdef BOOST_MSVC
    char constexpr path_separator = '\\';
    if(result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
    for(auto& c : result)
        if(c == '/')
            c = path_separator;
#else
    char constexpr path_separator = '/';
    if(result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
#endif
    return result;
}

// Return a response for the given request.
//
// The concrete type of the response message (which depends on the
// request), is type-erased in message_generator.
template<class Body, class Allocator>
http::message_generator
handle_request(
    beast::string_view doc_root,
    http::request<Body, http::basic_fields<Allocator>>&& req)
{
    // Returns a bad request response
    auto const bad_request =
    [&req](beast::string_view why)
    {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = std::string(why);
        res.prepare_payload();
        return res;
    };

    // Returns a not found response
    auto const not_found =
    [&req](beast::string_view target)
    {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + std::string(target) + "' was not found.";
        res.prepare_payload();
        return res;
    };

    // Returns a server error response
    auto const server_error =
    [&req](beast::string_view what)
    {
        http::response<http::string_body> res{http::status::internal_server_error, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + std::string(what) + "'";
        res.prepare_payload();
        return res;
    };

    // Make sure we can handle the method
    if( req.method() != http::verb::get &&
        req.method() != http::verb::head)
        return bad_request("Unknown HTTP-method");

    // Request path must be absolute and not contain "..".
    if( req.target().empty() ||
        req.target()[0] != '/' ||
        req.target().find("..") != beast::string_view::npos)
        return bad_request("Illegal request-target");

    // Build the path to the requested file
    std::string path = path_cat(doc_root, req.target());
    if(req.target().back() == '/')
        path.append("index.html");

    // Attempt to open the file
    beast::error_code ec;
    http::file_body::value_type body;
    body.open(path.c_str(), beast::file_mode::scan, ec);

    // Handle the case where the file doesn't exist
    if(ec == beast::errc::no_such_file_or_directory)
        return not_found(req.target());

    // Handle an unknown error
    if(ec)
        return server_error(ec.message());

    // Cache the size since we need it after the move
    auto const size = body.size();

    // Respond to HEAD request
    if(req.method() == http::verb::head)
    {
        http::response<http::empty_body> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, mime_type(path));
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        return res;
    }

    // Respond to GET request
    http::response<http::file_body> res{
        std::piecewise_construct,
        std::make_tuple(std::move(body)),
        std::make_tuple(http::status::ok, req.version())};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, mime_type(path));
    res.content_length(size);
    res.keep_alive(req.keep_alive());
    return res;
}

//------------------------------------------------------------------------------

// Report a failure
void
fail(beast::error_code ec, char const* what)
{
    // ssl::error::stream_truncated, also known as an SSL "short read",
    // indicates the peer closed the connection without performing the
    // required closing handshake (for example, Google does this to
    // improve performance). Generally this can be a security issue,
    // but if your communication protocol is self-terminated (as
    // it is with both HTTP and WebSocket) then you may simply
    // ignore the lack of close_notify.
    //
    // https://github.com/boostorg/beast/issues/38
    //
    // https://security.stackexchange.com/questions/91435/how-to-handle-a-malicious-ssl-tls-shutdown
    //
    // When a short read would cut off the end of an HTTP message,
    // Beast returns the error beast::http::error::partial_message.
    // Therefore, if we see a short read here, it has occurred
    // after the message has been completed, so it is safe to ignore it.

    if(ec == net::ssl::error::stream_truncated)
        return;

    std::cerr << what << ": " << ec.message() << "\n";
}

// A simple helper for cancellation_slot
struct cancellation_signals
{
    std::list<net::cancellation_signal> sigs;
    std::mutex mtx;
    void emit(net::cancellation_type ct = net::cancellation_type::all)
    {
        std::lock_guard<std::mutex> _(mtx);

        for (auto & sig : sigs)
            sig.emit(ct);
    }

    net::cancellation_slot slot()
    {
        std::lock_guard<std::mutex> _(mtx);

        auto itr = std::find_if(sigs.begin(), sigs.end(),
                             [](net::cancellation_signal & sig)
                             {
                                return !sig.slot().has_handler();
                             });

        if (itr != sigs.end())
            return itr->slot();
        else
            return sigs.emplace_back().slot();
    }
};

//------------------------------------------------------------------------------

// Echoes back all received WebSocket messages.
// This uses the Curiously Recurring Template Pattern so that
// the same code works with both SSL streams and regular sockets.
template<class Derived>
class websocket_session
{
    // Access the derived class, this is part of
    // the Curiously Recurring Template Pattern idiom.
    Derived&
    derived()
    {
        return static_cast<Derived&>(*this);
    }

    beast::flat_buffer buffer_;

    // Start the asynchronous operation
    template<class Body, class Allocator>
    void
    do_accept(http::request<Body, http::basic_fields<Allocator>> req)
    {
        // Set suggested timeout settings for the websocket
        derived().ws().set_option(
            websocket::stream_base::timeout::suggested(
                beast::role_type::server));

        // Set a decorator to change the Server of the handshake
        derived().ws().set_option(
            websocket::stream_base::decorator(
            [](websocket::response_type& res)
            {
                res.set(http::field::server,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " advanced-server-flex");
            }));

        // Accept the websocket handshake
        derived().ws().async_accept(
            req,
            beast::bind_front_handler(
                &websocket_session::on_accept,
                derived().shared_from_this()));
    }

private:
    void
    on_accept(beast::error_code ec)
    {
        if(ec)
            return fail(ec, "accept");

        // Read a message
        do_read();
    }

    void
    do_read()
    {
        // Read a message into our buffer
        derived().ws().async_read(
            buffer_,
            beast::bind_front_handler(
                &websocket_session::on_read,
                derived().shared_from_this()));
    }

    void
    on_read(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This indicates that the websocket_session was closed
        if(ec == websocket::error::closed)
            return;

        if(ec)
            return fail(ec, "read");

        // Echo the message
        derived().ws().text(derived().ws().got_text());
        derived().ws().async_write(
            buffer_.data(),
            beast::bind_front_handler(
                &websocket_session::on_write,
                derived().shared_from_this()));
    }

    void
    on_write(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
            return fail(ec, "write");

        // Clear the buffer
        buffer_.consume(buffer_.size());

        // Do another read
        do_read();
    }

public:
    // Start the asynchronous operation
    template<class Body, class Allocator>
    void
    run(http::request<Body, http::basic_fields<Allocator>> req)
    {
        // Accept the WebSocket upgrade request
        do_accept(std::move(req));
    }
};

//------------------------------------------------------------------------------

// Handles a plain WebSocket connection
class plain_websocket_session
    : public websocket_session<plain_websocket_session>
    , public std::enable_shared_from_this<plain_websocket_session>
{
    websocket::stream<beast::tcp_stream> ws_;

public:
    // Create the session
    explicit
    plain_websocket_session(
        beast::tcp_stream&& stream)
        : ws_(std::move(stream))
    {
    }

    // Called by the base class
    websocket::stream<beast::tcp_stream>&
    ws()
    {
        return ws_;
    }
};

//------------------------------------------------------------------------------

// Handles an SSL WebSocket connection
class ssl_websocket_session
    : public websocket_session<ssl_websocket_session>
    , public std::enable_shared_from_this<ssl_websocket_session>
{
    websocket::stream<
        beast::ssl_stream<beast::tcp_stream>> ws_;

public:
    // Create the ssl_websocket_session
    explicit
    ssl_websocket_session(
        beast::ssl_stream<beast::tcp_stream>&& stream)
        : ws_(std::move(stream))
    {
    }

    // Called by the base class
    websocket::stream<
        beast::ssl_stream<beast::tcp_stream>>&
    ws()
    {
        return ws_;
    }
};

//------------------------------------------------------------------------------

template<class Body, class Allocator>
void
make_websocket_session(
    beast::tcp_stream stream,
    http::request<Body, http::basic_fields<Allocator>> req)
{
    std::make_shared<plain_websocket_session>(
        std::move(stream))->run(std::move(req));
}

template<class Body, class Allocator>
void
make_websocket_session(
    beast::ssl_stream<beast::tcp_stream> stream,
    http::request<Body, http::basic_fields<Allocator>> req)
{
    std::make_shared<ssl_websocket_session>(
        std::move(stream))->run(std::move(req));
}

//------------------------------------------------------------------------------

// Handles an HTTP server connection.
// This uses the Curiously Recurring Template Pattern so that
// the same code works with both SSL streams and regular sockets.
template<class Derived>
class http_session
{
    std::shared_ptr<std::string const> doc_root_;

    // Access the derived class, this is part of
    // the Curiously Recurring Template Pattern idiom.
    Derived&
    derived()
    {
        return static_cast<Derived&>(*this);
    }

    static constexpr std::size_t queue_limit = 8; // max responses
    std::vector<http::message_generator> response_queue_;

    // The parser is stored in an optional container so we can
    // construct it from scratch it at the beginning of each new message.
    boost::optional<http::request_parser<http::string_body>> parser_;

protected:
    beast::flat_buffer buffer_;

public:
    // Construct the session
    http_session(
        beast::flat_buffer buffer,
        std::shared_ptr<std::string const> const& doc_root)
        : doc_root_(doc_root)
        , buffer_(std::move(buffer))
    {
    }

    void
    do_read()
    {
        // Construct a new parser for each message
        parser_.emplace();

        // Apply a reasonable limit to the allowed size
        // of the body in bytes to prevent abuse.
        parser_->body_limit(10000);

        // Set the timeout.
        beast::get_lowest_layer(
            derived().stream()).expires_after(std::chrono::seconds(30));

        // Read a request using the parser-oriented interface
        http::async_read(
            derived().stream(),
            buffer_,
            *parser_,
            beast::bind_front_handler(
                &http_session::on_read,
                derived().shared_from_this()));
    }

    void
    on_read(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if(ec == http::error::end_of_stream)
            return derived().do_eof();

        if(ec)
            return fail(ec, "read");

        // See if it is a WebSocket Upgrade
        if(websocket::is_upgrade(parser_->get()))
        {
            // Disable the timeout.
            // The websocket::stream uses its own timeout settings.
            beast::get_lowest_layer(derived().stream()).expires_never();

            // Create a websocket session, transferring ownership
            // of both the socket and the HTTP request.
            return make_websocket_session(
                derived().release_stream(),
                parser_->release());
        }

        // Send the response
        queue_write(handle_request(*doc_root_, parser_->release()));

        // If we aren't at the queue limit, try to pipeline another request
        if (response_queue_.size() < queue_limit)
            do_read();
    }

    void
    queue_write(http::message_generator response)
    {
        // Allocate and store the work
        response_queue_.push_back(std::move(response));

        // If there was no previous work, start the write
        // loop
        if (response_queue_.size() == 1)
            do_write();
    }

    // Called to start/continue the write-loop. Should not be called when
    // write_loop is already active.
    //
    // Returns `true` if the caller may initiate a new read
    bool
    do_write()
    {
        bool const was_full =
            response_queue_.size() == queue_limit;

        if(! response_queue_.empty())
        {
            http::message_generator msg =
                std::move(response_queue_.front());
            response_queue_.erase(response_queue_.begin());

            bool keep_alive = msg.keep_alive();

            beast::async_write(
                derived().stream(),
                std::move(msg),
                beast::bind_front_handler(
                    &http_session::on_write,
                    derived().shared_from_this(),
                    keep_alive));
        }

        return was_full;
    }

    void
    on_write(
        bool keep_alive,
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
            return fail(ec, "write");

        if(! keep_alive)
        {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            return derived().do_eof();
        }

        // Inform the queue that a write completed
        if(do_write())
        {
            // Read another request
            do_read();
        }
    }
};

//------------------------------------------------------------------------------

// Handles a plain HTTP connection
class plain_http_session
    : public http_session<plain_http_session>
    , public std::enable_shared_from_this<plain_http_session>
{
    beast::tcp_stream stream_;

public:
    // Create the session
    plain_http_session(
        beast::tcp_stream&& stream,
        beast::flat_buffer&& buffer,
        std::shared_ptr<std::string const> const& doc_root)
        : http_session<plain_http_session>(
            std::move(buffer),
            doc_root)
        , stream_(std::move(stream))
    {
    }

    // Start the session
    void
    run()
    {
        this->do_read();
    }

    // Called by the base class
    beast::tcp_stream&
    stream()
    {
        return stream_;
    }

    // Called by the base class
    beast::tcp_stream
    release_stream()
    {
        return std::move(stream_);
    }

    // Called by the base class
    void
    do_eof()
    {
        // Send a TCP shutdown
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

        // At this point the connection is closed gracefully
    }
};

//------------------------------------------------------------------------------

// Handles an SSL HTTP connection
class ssl_http_session
    : public http_session<ssl_http_session>
    , public std::enable_shared_from_this<ssl_http_session>
{
    beast::ssl_stream<beast::tcp_stream> stream_;

public:
    // Create the http_session
    ssl_http_session(
        beast::tcp_stream&& stream,
        ssl::context& ctx,
        beast::flat_buffer&& buffer,
        std::shared_ptr<std::string const> const& doc_root)
        : http_session<ssl_http_session>(
            std::move(buffer),
            doc_root)
        , stream_(std::move(stream), ctx)
    {
    }

    // Start the session
    void
    run()
    {
        // Set the timeout.
        beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

        // Perform the SSL handshake
        // Note, this is the buffered version of the handshake.
        stream_.async_handshake(
            ssl::stream_base::server,
            buffer_.data(),
            beast::bind_front_handler(
                &ssl_http_session::on_handshake,
                shared_from_this()));
    }

    // Called by the base class
    beast::ssl_stream<beast::tcp_stream>&
    stream()
    {
        return stream_;
    }

    // Called by the base class
    beast::ssl_stream<beast::tcp_stream>
    release_stream()
    {
        return std::move(stream_);
    }

    // Called by the base class
    void
    do_eof()
    {
        // Set the timeout.
        beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

        // Perform the SSL shutdown
        stream_.async_shutdown(
            beast::bind_front_handler(
                &ssl_http_session::on_shutdown,
                shared_from_this()));
    }

private:
    void
    on_handshake(
        beast::error_code ec,
        std::size_t bytes_used)
    {
        if(ec)
            return fail(ec, "handshake");

        // Consume the portion of the buffer used by the handshake
        buffer_.consume(bytes_used);

        do_read();
    }

    void
    on_shutdown(beast::error_code ec)
    {
        if(ec)
            return fail(ec, "shutdown");

        // At this point the connection is closed gracefully
    }
};

//------------------------------------------------------------------------------

// Detects SSL handshakes
class detect_session : public std::enable_shared_from_this<detect_session>
{
    beast::tcp_stream stream_;
    ssl::context& ctx_;
    std::shared_ptr<std::string const> doc_root_;
    beast::flat_buffer buffer_;

public:
    explicit
    detect_session(
        tcp::socket&& socket,
        ssl::context& ctx,
        std::shared_ptr<std::string const> const& doc_root)
        : stream_(std::move(socket))
        , ctx_(ctx)
        , doc_root_(doc_root)
    {
    }

    // Launch the detector
    void
    run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        net::dispatch(
            stream_.get_executor(),
            beast::bind_front_handler(
                &detect_session::on_run,
                this->shared_from_this()));
    }

    void
    on_run()
    {
        // Set the timeout.
        stream_.expires_after(std::chrono::seconds(30));

        beast::async_detect_ssl(
            stream_,
            buffer_,
            beast::bind_front_handler(
                &detect_session::on_detect,
                this->shared_from_this()));
    }

    void
    on_detect(beast::error_code ec, bool result)
    {
        if(ec)
            return fail(ec, "detect");

        if(result)
        {
            // Launch SSL session
            std::make_shared<ssl_http_session>(
                std::move(stream_),
                ctx_,
                std::move(buffer_),
                doc_root_)->run();
            return;
        }

        // Launch plain session
        std::make_shared<plain_http_session>(
            std::move(stream_),
            std::move(buffer_),
            doc_root_)->run();
    }
};

template<typename Stream>
net::awaitable<void, executor_type> do_eof(Stream & stream)
{
    beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_send, ec);
    co_return ;
}

template<typename Stream>
BOOST_ASIO_NODISCARD net::awaitable<void, executor_type>
do_eof(beast::ssl_stream<Stream> & stream)
{
    co_await stream.async_shutdown();
}


template<typename Stream, typename Body, typename Allocator>
BOOST_ASIO_NODISCARD net::awaitable<void, executor_type>
run_websocket_session(Stream & stream,
                      beast::flat_buffer & buffer,
                      http::request<Body, http::basic_fields<Allocator>> req,
                      const std::shared_ptr<std::string const> & doc_root)
{
    beast::websocket::stream<Stream&> ws{stream};

    // Set suggested timeout settings for the websocket
    ws.set_option(
            websocket::stream_base::timeout::suggested(
                    beast::role_type::server));

    // Set a decorator to change the Server of the handshake
    ws.set_option(
            websocket::stream_base::decorator(
                    [](websocket::response_type& res)
                    {
                        res.set(http::field::server,
                                std::string(BOOST_BEAST_VERSION_STRING) +
                            " advanced-server-flex");
                    }));

    // Accept the websocket handshake
    auto [ec] = co_await ws.async_accept(req);
    if (ec)
        co_return fail(ec, "accept");

    while (true)
    {


        // Read a message
        std::size_t bytes_transferred = 0u;
        std::tie(ec, bytes_transferred) = co_await ws.async_read(buffer);

        // This indicates that the websocket_session was closed
        if (ec == websocket::error::closed)
            co_return;
        if (ec)
            co_return fail(ec, "read");

        ws.text(ws.got_text());
        std::tie(ec, bytes_transferred) = co_await ws.async_write(buffer.data());

        if (ec)
            co_return fail(ec, "write");

        // Clear the buffer
        buffer.consume(buffer.size());
    }
}


template<typename Stream>
BOOST_ASIO_NODISCARD net::awaitable<void, executor_type>
run_session(Stream & stream, beast::flat_buffer & buffer, const std::shared_ptr<std::string const> & doc_root)
{
    http::request_parser<http::string_body> parser;
    // Apply a reasonable limit to the allowed size
    // of the body in bytes to prevent abuse.
    parser.body_limit(10000);

    auto [ec, bytes_transferred] = co_await http::async_read(stream, buffer, parser);

    if(ec == http::error::end_of_stream)
        co_await do_eof(stream);

    if(ec)
        co_return fail(ec, "read");

    // this can be
    // while ((co_await net::this_coro::cancellation_state).cancelled() == net::cancellation_type::none)
    // on most compilers
    for (auto cs = co_await net::this_coro::cancellation_state;
         cs.cancelled() == net::cancellation_type::none;
         cs = co_await net::this_coro::cancellation_state)
    {
        if(websocket::is_upgrade(parser.get()))
        {
            // Disable the timeout.
            // The websocket::stream uses its own timeout settings.
            beast::get_lowest_layer(stream).expires_never();

            co_await run_websocket_session(stream, buffer, parser.release(), doc_root);
            co_return ;
        }

        // we follow a different strategy then the other example: instead of queue responses,
        // we always to one read & write in parallel.

        auto res = handle_request(*doc_root, parser.release());
        if (!res.keep_alive())
        {
            http::message_generator msg{std::move(res)};
            auto [ec, sz] = co_await beast::async_write(stream, std::move(msg));
            if (ec)
                fail(ec, "write");
            co_return ;
        }

        http::message_generator msg{std::move(res)};

        auto [_, ec_r, sz_r, ec_w, sz_w ] =
                co_await net::experimental::make_parallel_group(
                    http::async_read(stream, buffer, parser, net::deferred),
                    beast::async_write(stream, std::move(msg), net::deferred))
                        .async_wait(net::experimental::wait_for_all(),
                                    net::as_tuple(net::use_awaitable_t<executor_type>{}));

        if (ec_r)
            co_return fail(ec_r, "read");

        if (ec_w)
            co_return fail(ec_w, "write");

    }


}

BOOST_ASIO_NODISCARD net::awaitable<void, executor_type>
detect_session(typename beast::tcp_stream::rebind_executor<executor_with_default>::other stream,
               net::ssl::context & ctx,
               std::shared_ptr<std::string const> doc_root)
{
    beast::flat_buffer buffer;

    // Set the timeout.
    stream.expires_after(std::chrono::seconds(30));
    // on_run
    auto [ec, result] = co_await beast::async_detect_ssl(stream, buffer);
    // on_detect
    if (ec)
        co_return fail(ec, "detect");

    if(result)
    {
        using stream_type = typename beast::tcp_stream::rebind_executor<executor_with_default>::other;
        beast::ssl_stream<stream_type> ssl_stream{std::move(stream), ctx};
        auto [ec, bytes_used] = co_await ssl_stream.async_handshake(net::ssl::stream_base::server, buffer.data());

        if(ec)
            co_return fail(ec, "handshake");

        buffer.consume(bytes_used);
        co_await run_session(ssl_stream, buffer, doc_root);
    }
    else
        co_await run_session(stream, buffer, doc_root);


}

bool init_listener(typename tcp::acceptor::rebind_executor<executor_with_default>::other & acceptor,
                   const tcp::endpoint &endpoint)
{
    beast::error_code ec;
    // Open the acceptor
    acceptor.open(endpoint.protocol(), ec);
    if(ec)
    {
        fail(ec, "open");
        return false;
    }

    // Allow address reuse
    acceptor.set_option(net::socket_base::reuse_address(true), ec);
    if(ec)
    {
        fail(ec, "set_option");
        return false;
    }

    // Bind to the server address
    acceptor.bind(endpoint, ec);
    if(ec)
    {
        fail(ec, "bind");
        return false;
    }

    // Start listening for connections
    acceptor.listen(
            net::socket_base::max_listen_connections, ec);
    if(ec)
    {
        fail(ec, "listen");
        return false;
    }
    return true;

}

// Accepts incoming connections and launches the sessions.
BOOST_ASIO_NODISCARD net::awaitable<void, executor_type>
            listen(ssl::context& ctx,
                   tcp::endpoint endpoint,
                   std::shared_ptr<std::string const> doc_root,
                   cancellation_signals & sig)
{
    typename tcp::acceptor::rebind_executor<executor_with_default>::other acceptor{co_await net::this_coro::executor};
    if (!init_listener(acceptor, endpoint))
        co_return;

    while ((co_await net::this_coro::cancellation_state).cancelled() == net::cancellation_type::none)
    {
        auto [ec, sock] = co_await acceptor.async_accept();
        const auto exec = sock.get_executor();
        using stream_type = typename beast::tcp_stream::rebind_executor<executor_with_default>::other;
        if (!ec)
            // We dont't need a strand, since the awaitable is an implicit strand.
            net::co_spawn(exec,
                          detect_session(stream_type(std::move(sock)), ctx, doc_root),
                          net::bind_cancellation_slot(sig.slot(), net::detached));
    }
}

//------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    // Check command line arguments.
    if (argc != 5)
    {
        std::cerr <<
            "Usage: advanced-server-flex-awaitable <address> <port> <doc_root> <threads>\n" <<
            "Example:\n" <<
            "    advanced-server-flex-awaitable 0.0.0.0 8080 . 1\n";
        return EXIT_FAILURE;
    }
    auto const address = net::ip::make_address(argv[1]);
    auto const port = static_cast<unsigned short>(std::atoi(argv[2]));
    auto const doc_root = std::make_shared<std::string>(argv[3]);
    auto const threads = std::max<int>(1, std::atoi(argv[4]));

    // The io_context is required for all I/O
    net::io_context ioc{threads};

    // The SSL context is required, and holds certificates
    ssl::context ctx{ssl::context::tlsv12};

    // This holds the self-signed certificate used by the server
    load_server_certificate(ctx);

    // Cancellation-signal for SIGINT
    cancellation_signals cancellation;

    // Create and launch a listening routine
    net::co_spawn(
            ioc,
            listen(ctx, tcp::endpoint{address, port}, doc_root, cancellation),
            net::bind_cancellation_slot(cancellation.slot(), net::detached));


    // Capture SIGINT and SIGTERM to perform a clean shutdown
    net::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait(
        [&](beast::error_code const&, int sig)
        {
            if (sig == SIGINT)
                cancellation.emit(net::cancellation_type::all);
            else
            {
                // Stop the `io_context`. This will cause `run()`
                // to return immediately, eventually destroying the
                // `io_context` and all of the sockets in it.
                ioc.stop();
            }
        });

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for(auto i = threads - 1; i > 0; --i)
        v.emplace_back(
        [&ioc]
        {
            ioc.run();
        });
    ioc.run();

    // (If we get here, it means we got a SIGINT or SIGTERM)

    // Block until all the threads exit
    for(auto& t : v)
        t.join();

    return EXIT_SUCCESS;
}


#else

int main(int, char * [])
{
    std::printf("awaitables require C++20\n");
    return 1;
}

#endif
