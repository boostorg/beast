//
// Copyright (c) 2025 Mungo Gill
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance
//

#include "processor_base.hpp"
#include "historic_price_fetcher.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>

#include <iostream>
#include <functional>
#include <string>

using namespace boost;
using namespace std::placeholders;

using namespace beast;         // from <boost/beast.hpp>

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

// Start the asynchronous operation
void historic_price_fetcher::run()
{
    // We use a fixed host
    host_ = "api.coinbase.com";
    //host_ = "ws-feed.exchange.coinbase.com";

    // Set SNI Hostname (many hosts need this to handshake successfully)
    if (!SSL_set_tlsext_host_name(stream_.native_handle(), host_.c_str()))
    {
        beast::error_code ec{
            static_cast<int>(::ERR_get_error()),
            net::error::get_ssl_category() };
        return error_handler_(ec, "SNI");
    }

    // Set the expected hostname in the peer certificate for verification
    stream_.set_verify_callback(boost::asio::ssl::host_name_verification(host_));

    // Set up an HTTP GET request message
    req_.version(11);
    req_.method(http::verb::get);
    req_.set(http::field::host, host_);
    req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    active_ = true;

    // Look up the domain name
    resolver_.async_resolve(
        host_,
        "443",
        [this](error_code ec, tcp::resolver::results_type results)
        {
            on_resolve(ec, results);
        }
    );
}

void historic_price_fetcher::on_write(beast::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    if (ec) {
        cancel();
        return error_handler_(ec, "write");
    }

    if (!active_)
        return;

    // Read a message into our buffer
    http::async_read(
        stream_, buffer_, response_,
        [this](error_code ec, std::size_t bytes_transferred)
        {
            on_read(ec, bytes_transferred);
        }
    );
}

void historic_price_fetcher::next_request()
{
    if (coins_.size() > 0)
    {
        std::string security = coins_.back();
        coins_.pop_back();

        // Set up an HTTP GET request message
        req_.target("/v2/prices/" + security + "/spot");

        // Set a timeout on the operation
        beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

        // Send the message
        http::async_write(stream_, req_,
            [this](error_code ec, std::size_t bytes_transferred)
            {
                on_write(ec, bytes_transferred);
            }
        );
    }
    else {
        cancel();
    }
}

void historic_price_fetcher::on_ssl_handshake(beast::error_code ec)
{
    if (ec) {
        cancel();
        return error_handler_(ec, "ssl_handshake");
    }

    if (!active_)
        return;

    if (coins_.size() > 0)
        next_request();
    else {
        cancel();
    }
}

void historic_price_fetcher::on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep)
{
    boost::ignore_unused(ep);

    if (ec) {
        cancel();
        return error_handler_(ec, "connect");
    }

    if (!active_)
        return;

    // Set a timeout on the operation
    beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

    // Perform the SSL handshake
    stream_.async_handshake(
        boost::asio::ssl::stream_base::client,
        [this](error_code ec)
        {
            on_ssl_handshake(ec);
        }
    );
}

void historic_price_fetcher::on_resolve(beast::error_code ec, tcp::resolver::results_type results)
{
    if (ec) {
        cancel();
        return error_handler_(ec, "resolve");
    }

    if (!active_)
        return;

    // Set a timeout on the operation
    beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

    // Make the connection on the IP address we get from a lookup
    beast::get_lowest_layer(stream_).async_connect(
        results,
        [this](error_code ec, tcp::resolver::results_type::endpoint_type ep)
        {
            on_connect(ec, ep);
        }
    );
}

void historic_price_fetcher::cancel()
{
    active_ = false;

    net::post(strand_, [this]()
        {
            if (beast::get_lowest_layer(stream_).socket().is_open())
                stream_.async_shutdown(
                    [this](error_code ec)
                    {
                        on_shutdown(ec);
                    }
                );
        }
    );
}

void historic_price_fetcher::on_read(beast::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    // This indicates that the session was closed
    if (ec == http::error::end_of_stream) {
        cancel();
        return;
    }
    else if (ec) {
        cancel();
        if (active_) return error_handler_(ec, "read");
    }

    if (!active_)
        return;

    // Write the message to standard out
    //std::cout << "Response: " << response_ << "\n" << std::endl;
    //std::cout << "Body: " << response_.body() << "\n\n" << std::endl;

    // The response can be quite long, and we can avoid an allocation
    // by swapping the body into an empty string.
    std::string temp;
    temp.swap(response_.body());

    receive_handler_(std::move(temp));

    next_request();

    //// Set a timeout on the operation
    //beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

    //// Gracefully close the stream
    //stream_.async_shutdown(
    //    [this](error_code ec)
    //    {
    //        on_shutdown(ec);
    //    }
    //);

    //receive_handler_(beast::buffers_to_string(buffer_.data()));

    /*
    buffer_.consume(buffer_.max_size());

    count++;
    if (count > 3) cancel();

    ws_.async_read(
    buffer_,
    [this] (error_code ec, std::size_t bytes_transferred)
    {
    on_read(ec, bytes_transferred);
    }
    );
    */
}

void historic_price_fetcher::on_shutdown(beast::error_code ec)
{
    if (ec && ec != boost::asio::ssl::error::stream_truncated)
        return error_handler_(ec, "shutdown");

    // If we get here then the connection is closed gracefully

    // The make_printable() function helps print a ConstBufferSequence
    std::cout << "Final buffer content:" << beast::make_printable(buffer_.data()) << std::endl;
}
