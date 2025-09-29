//
// Copyright (c) 2025 Mungo Gill
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance
//

#ifndef BOOST_BEAST_EXAMPLE_LIVE_PRICE_LISTENER
#define BOOST_BEAST_EXAMPLE_LIVE_PRICE_LISTENER

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <string>

#include "processor_base.hpp"

//using namespace boost;

// Opens a websocket and subsscribes to price ticks
class live_price_listener : public processor_base
{
    // This holds the function called when a live price is received.
    std::function<void(std::string&&)> receive_handler_;

    // This holds the function called when an error happens.
    std::function<void(boost::beast::error_code, char const*)> error_handler_;

    // We want to ensure that operations to set up the websocket are performed in order,
    // and that we do not attempt to call async_read when another async_read is in progress.
    // This strand needs to persist as long as the websocket is in use.
    // Design note: we could "get away" without using a strand, because this class is structured
    // so that the next async_* function is not called until *after* the previous asynchronous
    // function is complete. However the strand is included since it makes our threading assumptions
    // explicit for future developers.
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;

    // The resolver's role is to perform DNS lookups from a hostname to a set of ip addresses.
    boost::asio::ip::tcp::resolver resolver_;

    // The key structure in this listener is the websocket itself.
    boost::beast::websocket::stream<boost::asio::ssl::stream<boost::beast::tcp_stream>> ws_;

    // Any calls to async_read need to be passed a buffer into which the response will
    // be written. That buffer needs to persist until after the read completes and the
    // completion handler is called.
    boost::beast::flat_buffer buffer_;

    // The host will be used at multiple stages during the websocket's setup process.
    std::string host_;

    // A list of coins that we want to get the prices for.
    std::vector<std::string> coins_;

    // Provide a mechanism to exit the websocket subscription. When active_ is false, 
    // no more asynchronous calls will be made, and every completion handler called will exit
    // immediately.
    bool active_;

    int count = 0;

public:
    // It is worth noting that we do not retain the reference to the passed-in io_context.
    // The reason for this is that we use a strand to ensure the 
    explicit
        live_price_listener(
            boost::asio::io_context& ioc
            , boost::asio::ssl::context& ctx
            , const std::vector<std::string>& coins
            , std::function<void(std::string&&)> receive_handler
            , std::function<void(boost::beast::error_code, char const*)> err_handler)
        : receive_handler_(receive_handler)
        , error_handler_(err_handler)
        , strand_(boost::asio::make_strand(ioc))
        , resolver_(strand_)
        , ws_(strand_, ctx)
        , coins_(coins)
        , active_(false)
    {
        // For this example hard-code the host.
        //host_ = "ws-feed-public.sandbox.exchange.coinbase.com";
        host_ = "ws-feed.exchange.coinbase.com";
    }

    // Start the asynchronous operation
    void
        run();

    void
        cancel() override;

private:
    void
        on_resolve(
            boost::beast::error_code ec,
            boost::asio::ip::tcp::resolver::results_type results);

    void
        on_connect(
            boost::beast::error_code ec,
            boost::asio::ip::tcp::resolver::results_type::endpoint_type ep);

    void
        on_ssl_handshake(boost::beast::error_code ec);

    void
        on_handshake(boost::beast::error_code ec);

    void
        on_write(
            boost::beast::error_code ec,
            std::size_t bytes_required,
            std::size_t bytes_transferred);

    void
        on_read(
            boost::beast::error_code ec,
            std::size_t bytes_transferred);

    void
        on_close(boost::beast::error_code ec);
};

#endif
