//
// Copyright (c) 2025 Mungo Gill
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance
//

#ifndef BOOST_BEAST_EXAMPLE_HISTORIC_PRICE_FETCHER
#define BOOST_BEAST_EXAMPLE_HISTORIC_PRICE_FETCHER

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <string>

#include "processor_base.hpp"


//using namespace boost;

// Opens a websocket and subsscribes to price ticks
class historic_price_fetcher : public processor_base
{
    std::function<void(std::string&&)> receive_handler_;
    std::function<void(boost::beast::error_code, char const*)> error_handler_;

    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::ssl::stream<boost::beast::tcp_stream> stream_;

    boost::beast::flat_buffer buffer_;
    boost::beast::http::request<boost::beast::http::string_body> req_;
    boost::beast::http::response<boost::beast::http::string_body> response_;

    std::string host_;
    std::vector<std::string> coins_ = { "BTC-USD", "ETH-USD" };

    bool active_;

public:
    // Resolver and socket require an io_context
    explicit
        historic_price_fetcher(
            boost::asio::io_context& ioc
            , boost::asio::ssl::context& ctx
            , const std::vector<std::string>& coins
            , std::function<void(std::string&&)> receive_handler
            , std::function<void(boost::beast::error_code, char const*)> err_handler)
        : receive_handler_(receive_handler)
        , error_handler_(err_handler)
        , strand_(boost::asio::make_strand(ioc))
        , resolver_(strand_)
        , stream_(strand_, ctx)
        , coins_(coins)
        , active_(false)
    {
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
        next_request();

    void
        on_write(
            boost::beast::error_code ec,
            std::size_t bytes_transferred);

    void
        on_read(
            boost::beast::error_code ec,
            std::size_t bytes_transferred);

    void
        on_shutdown(boost::beast::error_code ec);
};

#endif
