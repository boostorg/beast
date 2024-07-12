//
// Copyright (c) 2022 Klemens D. Morgenstern (klemens dot morgenstern at gmx dot net)
// Copyright (c) 2024 Mohammad Nejati
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

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/scope/scope_exit.hpp>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <list>
#include <string>
#include <thread>
#include <vector>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

namespace beast     = boost::beast;
namespace http      = beast::http;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
namespace ssl       = boost::asio::ssl;

using executor_type = net::strand<net::io_context::executor_type>;
using stream_type   = typename beast::tcp_stream::rebind_executor<executor_type>::other;
using acceptor_type = typename net::ip::tcp::acceptor::rebind_executor<executor_type>::other;

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

/** A thread-safe task group that tracks child tasks, allows emitting
    cancellation signals to them, and waiting for their completion.
*/
class task_group
{
    std::mutex mtx_;
    net::steady_timer cv_;
    std::list<net::cancellation_signal> css_;

public:
    task_group(net::any_io_executor exec)
        : cv_{ std::move(exec), net::steady_timer::time_point::max() }
    {
    }

    task_group(task_group const&) = delete;
    task_group(task_group&&)      = delete;

    /** Adds a cancellation slot and a wrapper object that will remove the child
        task from the list when it completes.

        @param completion_token The completion token that will be adapted.

        @par Thread Safety
        @e Distinct @e objects: Safe.@n
        @e Shared @e objects: Safe.
    */
    template<typename CompletionToken>
    auto
    adapt(CompletionToken&& completion_token)
    {
        auto lg = std::lock_guard{ mtx_ };
        auto cs = css_.emplace(css_.end());

        return net::bind_cancellation_slot(
            cs->slot(),
            net::consign(
                std::forward<CompletionToken>(completion_token),
                boost::scope::make_scope_exit(
                    [this, cs]()
                    {
                        auto lg = std::lock_guard{ mtx_ };
                        if(css_.erase(cs) == css_.end())
                            cv_.cancel();
                    })));
    }

    /** Emits the signal to all child tasks and invokes the slot's
        handler, if any.

        @param type The completion type that will be emitted to child tasks.

        @par Thread Safety
        @e Distinct @e objects: Safe.@n
        @e Shared @e objects: Safe.
    */
    void
    emit(net::cancellation_type type)
    {
        auto lg = std::lock_guard{ mtx_ };
        for(auto& cs : css_)
            cs.emit(type);
    }

    /** Starts an asynchronous wait on the task_group.

        The completion handler will be called when:

        @li All the child tasks completed.
        @li The operation was cancelled.

        @param completion_token The completion token that will be used to
        produce a completion handler. The function signature of the completion
        handler must be:
        @code
        void handler(
            boost::system::error_code const& error  // result of operation
        );
        @endcode

        @par Thread Safety
        @e Distinct @e objects: Safe.@n
        @e Shared @e objects: Safe.
    */
    template<
        typename CompletionToken =
            net::default_completion_token_t<net::any_io_executor>>
    auto
    async_wait(
        CompletionToken&& completion_token =
            net::default_completion_token_t<net::any_io_executor>{})
    {
        return net::
            async_compose<CompletionToken, void(boost::system::error_code)>(
                [this, scheduled = false](
                    auto&& self, boost::system::error_code ec = {}) mutable
                {
                    if(!scheduled)
                        self.reset_cancellation_state(
                            net::enable_total_cancellation());

                    if(!self.cancelled() && ec == net::error::operation_aborted)
                        ec = {};

                    {
                        auto lg = std::lock_guard{ mtx_ };

                        if(!css_.empty() && !ec)
                        {
                            scheduled = true;
                            return cv_.async_wait(std::move(self));
                        }
                    }

                    if(!std::exchange(scheduled, true))
                        return net::post(net::append(std::move(self), ec));

                    self.complete(ec);
                },
                completion_token,
                cv_);
    }
};

template<typename Stream>
net::awaitable<void, executor_type>
run_websocket_session(
    Stream& stream,
    beast::flat_buffer& buffer,
    http::request<http::string_body> req,
    beast::string_view doc_root)
{
    auto cs = co_await net::this_coro::cancellation_state;
    auto ws = websocket::stream<Stream&>{ stream };

    // Set suggested timeout settings for the websocket
    ws.set_option(
        websocket::stream_base::timeout::suggested(beast::role_type::server));

    // Set a decorator to change the Server of the handshake
    ws.set_option(websocket::stream_base::decorator(
        [](websocket::response_type& res)
        {
            res.set(
                http::field::server,
                std::string(BOOST_BEAST_VERSION_STRING) +
                    " advanced-server-flex");
        }));

    // Accept the websocket handshake
    co_await ws.async_accept(req);

    while(!cs.cancelled())
    {
        // Read a message
        auto [ec, _] = co_await ws.async_read(buffer, net::as_tuple);

        if(ec == websocket::error::closed || ec == ssl::error::stream_truncated)
            co_return;

        if(ec)
            throw boost::system::system_error{ ec };

        // Echo the message back
        ws.text(ws.got_text());
        co_await ws.async_write(buffer.data());

        // Clear the buffer
        buffer.consume(buffer.size());
    }

    // A cancellation has been requested, gracefully close the session.
    auto [ec] = co_await ws.async_close(
        websocket::close_code::service_restart, net::as_tuple);

    if(ec && ec != ssl::error::stream_truncated)
        throw boost::system::system_error{ ec };
}

template<typename Stream>
net::awaitable<void, executor_type>
run_session(
    Stream& stream,
    beast::flat_buffer& buffer,
    beast::string_view doc_root)
{
    auto cs = co_await net::this_coro::cancellation_state;

    while(!cs.cancelled())
    {
        http::request_parser<http::string_body> parser;
        parser.body_limit(10000);

        auto [ec, _] =
            co_await http::async_read(stream, buffer, parser, net::as_tuple);

        if(ec == http::error::end_of_stream)
            co_return;

        if(websocket::is_upgrade(parser.get()))
        {
            // The websocket::stream uses its own timeout settings.
            beast::get_lowest_layer(stream).expires_never();

            co_await run_websocket_session(
                stream, buffer, parser.release(), doc_root);

            co_return;
        }

        auto res = handle_request(doc_root, parser.release());
        if(!res.keep_alive())
        {
            co_await beast::async_write(stream, std::move(res));
            co_return;
        }

        co_await beast::async_write(stream, std::move(res));
    }
}

net::awaitable<void, executor_type>
detect_session(
    stream_type stream,
    ssl::context& ctx,
    beast::string_view doc_root)
{
    beast::flat_buffer buffer;

    // Allow total cancellation to change the cancellation state of this
    // coroutine, but only allow terminal cancellation to propagate to async
    // operations. This setting will be inherited by all child coroutines.
    co_await net::this_coro::reset_cancellation_state(
        net::enable_total_cancellation(), net::enable_terminal_cancellation());

    // We want to be able to continue performing new async operations, such as
    // cleanups, even after the coroutine is cancelled. This setting will be
    // inherited by all child coroutines.
    co_await net::this_coro::throw_if_cancelled(false);

    stream.expires_after(std::chrono::seconds(30));

    if(co_await beast::async_detect_ssl(stream, buffer))
    {
        ssl::stream<stream_type> ssl_stream{ std::move(stream), ctx };

        auto bytes_transferred = co_await ssl_stream.async_handshake(
            ssl::stream_base::server, buffer.data());

        buffer.consume(bytes_transferred);

        co_await run_session(ssl_stream, buffer, doc_root);

        if(!ssl_stream.lowest_layer().is_open())
            co_return;

        // Gracefully close the stream
        auto [ec] = co_await ssl_stream.async_shutdown(net::as_tuple);
        if(ec && ec != ssl::error::stream_truncated)
            throw boost::system::system_error{ ec };
    }
    else
    {
        co_await run_session(stream, buffer, doc_root);

        if(!stream.socket().is_open())
            co_return;

        stream.socket().shutdown(net::ip::tcp::socket::shutdown_send);
    }
}

net::awaitable<void, executor_type>
listen(
    task_group& task_group,
    ssl::context& ctx,
    net::ip::tcp::endpoint endpoint,
    beast::string_view doc_root)
{
    auto cs       = co_await net::this_coro::cancellation_state;
    auto executor = co_await net::this_coro::executor;
    auto acceptor = acceptor_type{ executor, endpoint };

    // Allow total cancellation to propagate to async operations.
    co_await net::this_coro::reset_cancellation_state(
        net::enable_total_cancellation());

    while(!cs.cancelled())
    {
        auto socket_executor = net::make_strand(executor.get_inner_executor());
        auto [ec, socket] =
            co_await acceptor.async_accept(socket_executor, net::as_tuple);

        if(ec == net::error::operation_aborted)
            co_return;

        if(ec)
            throw boost::system::system_error{ ec };

        net::co_spawn(
            std::move(socket_executor),
            detect_session(stream_type{ std::move(socket) }, ctx, doc_root),
            task_group.adapt(
                [](std::exception_ptr e)
                {
                    if(e)
                    {
                        try
                        {
                            std::rethrow_exception(e);
                        }
                        catch(std::exception& e)
                        {
                            std::cerr << "Error in session: " << e.what() << "\n";
                        }
                    }
                }));
    }
}

net::awaitable<void, executor_type>
handle_signals(task_group& task_group)
{
    auto executor   = co_await net::this_coro::executor;
    auto signal_set = net::signal_set{ executor, SIGINT, SIGTERM };

    auto sig = co_await signal_set.async_wait();

    if(sig == SIGINT)
    {
        std::cout << "Gracefully cancelling child tasks...\n";
        task_group.emit(net::cancellation_type::total);

        // Wait a limited time for child tasks to gracefully cancell
        auto [ec] = co_await task_group.async_wait(
            net::as_tuple(net::cancel_after(std::chrono::seconds{ 10 })));

        if(ec == net::error::operation_aborted) // Timeout occurred
        {
            std::cout << "Sending a terminal cancellation signal...\n";
            task_group.emit(net::cancellation_type::terminal);
            co_await task_group.async_wait();
        }

        std::cout << "Child tasks completed.\n";
    }
    else // SIGTERM
    {
        executor.get_inner_executor().context().stop();
    }
}

int
main(int argc, char* argv[])
{
    // Check command line arguments.
    if(argc != 5)
    {
        std::cerr << "Usage: advanced-server-flex-awaitable <address> <port> <doc_root> <threads>\n"
                  << "Example:\n"
                  << "    advanced-server-flex-awaitable 0.0.0.0 8080 . 1\n";
        return EXIT_FAILURE;
    }
    auto const address  = net::ip::make_address(argv[1]);
    auto const port     = static_cast<unsigned short>(std::atoi(argv[2]));
    auto const endpoint = net::ip::tcp::endpoint{ address, port };
    auto const doc_root = beast::string_view{ argv[3] };
    auto const threads  = std::max<int>(1, std::atoi(argv[4]));

    // The io_context is required for all I/O
    net::io_context ioc{ threads };

    // The SSL context is required, and holds certificates
    ssl::context ctx{ ssl::context::tlsv12 };

    // This holds the self-signed certificate used by the server
    load_server_certificate(ctx);

    // Track coroutines
    task_group task_group{ ioc.get_executor() };

    // Create and launch a listening coroutine
    net::co_spawn(
        net::make_strand(ioc),
        listen(task_group, ctx, endpoint, doc_root),
        task_group.adapt(
            [](std::exception_ptr e)
            {
                if(e)
                {
                    try
                    {
                        std::rethrow_exception(e);
                    }
                    catch(std::exception& e)
                    {
                        std::cerr << "Error in listener: " << e.what() << "\n";
                    }
                }
            }));

    // Create and launch a signal handler coroutine
    net::co_spawn(
        net::make_strand(ioc), handle_signals(task_group), net::detached);

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for(auto i = threads - 1; i > 0; --i)
        v.emplace_back([&ioc] { ioc.run(); });
    ioc.run();

    // Block until all the threads exit
    for(auto& t : v)
        t.join();

    return EXIT_SUCCESS;
}

#else

int
main(int, char*[])
{
    std::printf("awaitables require C++20\n");
    return EXIT_FAILURE;
}

#endif
