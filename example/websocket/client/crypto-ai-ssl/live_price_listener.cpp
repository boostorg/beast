//
// Copyright (c) 2025 Mungo Gill
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance
//

#include "processor_base.hpp"
#include "live_price_listener.hpp"

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
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/json.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>

#include <iostream>
#include <functional>


using namespace boost;
using namespace std::placeholders;

using namespace beast;         // from <boost/beast.hpp>
using namespace http;           // from <boost/beast/http.hpp>
using namespace websocket; // from <boost/beast/websocket.hpp>

namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>


static inline std::time_t my_time_gm(struct tm* tm) {
#if defined(_DEFAULT_SOURCE) // Feature test for glibc
    return timegm(tm);
#elif defined(_MSC_VER) // Test for Microsoft C/C++
    return _mkgmtime(tm);
#else
#error "Neither timegm nor _mkgmtime available"
#endif
}

// Start the asynchronous operation
void live_price_listener::run()
{


    // Set SNI Hostname (many hosts need this to handshake successfully)
    // Note that ws_.next_layer() references the asio::ssl::stream object.
    // Note, SSL_set_tlsext_host_name is an OpenSSL C function, not
    // part of asio or beast.
    if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host_.c_str()))
    {
        beast::error_code ec{
            static_cast<int>(::ERR_get_error()),
            net::error::get_ssl_category() };
        return error_handler_(ec, "SNI");
    }

    // Set the expected hostname in the peer certificate for verification.
    // OpenSSL will, whenever a certificate is received, call this function
    // to check that the certificate's host matches what we think it should be.
    ws_.next_layer().set_verify_callback(boost::asio::ssl::host_name_verification(host_));

    // Ensure any future callbacks do not early-exit.
    // (design note: could also have been done at construction time).
    active_ = true;

    // Request that ASIO lookup the domain name. For the sake of this example we have
    // hard-coded the port to 443 (the usual port for this).
    // For contrast with other examples, this has been written using a lambda, but
    // `beast::bind_front_handler` is an equally viable alternative.
    resolver_.async_resolve(
        host_,
        "https",
        [this](error_code ec, tcp::resolver::results_type results)
        {
            // Note that `results` is actually an iterator into a container of
            // endpoints representing all the IP addresses found by the DNS lookup.
            on_resolve(ec, results);
        }
    );
}

void live_price_listener::cancel()
{
    // We set active_=false to rapidly consume all the pending
    // completion handlers.
    active_ = false;

    // We use net::post to ensure the websocket closure takes place after
    // all the currently pending completion handlers.
    net::post(strand_, [this]()
        {
            // If the websocket is still open, close it.
            if (ws_.is_open())
                ws_.async_close(websocket::close_code::normal,
                    [this](error_code ec)
                    {
                        on_close(ec);
                    }
                );
        }
    );
}

// This is the function called when hostname resolution completes.
void live_price_listener::on_resolve(beast::error_code ec, tcp::resolver::results_type results)
{
    // In the event of an error call the `cancel` function which will drain
    // any pending completion handlers. In this case the websocket is not yet
    // open so the `cancel` function will not attempt to close it.
    if (ec) {
        cancel();
        return error_handler_(ec, "resolve");
    }

    // If we have been asked to shut down then do no processing.
    if (!active_)
        return;

    // Set the timeout for the operation. Note that this needs to be set
    // each time to reset the countdown.
    // This is applied on the underlying socket because neither the ssl
    // layer nor the websocket layer have been started yet.
    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

    // Make the connection on on of the IP address we got from the lookup.
    // If multiple IP addresses were found then the first one to sucessfully
    // connect is used.
    // ws_ has 3 layers websocket->ssl->socket
    // `get_lowest_layer` returns the bottom-most socket.
    beast::get_lowest_layer(ws_).async_connect(
        results,
        [this](error_code ec, tcp::resolver::results_type::endpoint_type ep)
        {
            // Note that the endpoint `ep` represents the single IP to which
            // we successfully connected (if any).
            on_connect(ec, ep);
        }
    );
}

// Once the underlying socket is connected, this function performs the next step,
// namely getting the SSL layer running.
void live_price_listener::on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep)
{
    if (ec) {
        // In the event of a connection error call the `cancel` function which will drain
        // any pending completion handlers. In this case the websocket is not yet
        // open so the `cancel` function will not attempt to close it.
        cancel();
        return error_handler_(ec, "connect");
    }

    // If we have been asked to shut down then do no further processing.
    if (!active_)
        return;

    // Set the timeout for the operation. Note that this needs to be set
    // each time to reset the countdown.
    // This is applied on the underlying socket because neither the ssl
    // layer nor the websocket layer have been started yet.
    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

    // Update the host_ string to add the port. This will provide the value of the
    // Host HTTP header during the WebSocket handshake.
    // See https://tools.ietf.org/html/rfc7230#section-5.4
    host_ += ':' + std::to_string(ep.port());

    // Perform the SSL handshake
    // ws_ has 3 layers websocket->ssl->socket
    // `get_next_layer` returns the ssl layer.
    ws_.next_layer().async_handshake(
        boost::asio::ssl::stream_base::client,
        [this](error_code ec)
        {
            on_ssl_handshake(ec);
        }
    );
}

void live_price_listener::on_ssl_handshake(beast::error_code ec)
{
    if (ec) {
        // In the event of an ssl error call the `cancel` function which will drain
        // any pending completion handlers. In this case the websocket is not yet
        // open so the `cancel` function will not attempt to close it.
        cancel();
        return error_handler_(ec, "ssl_handshake");
    }

    // If we have been asked to shut down then do no further processing.
    if (!active_)
        return;

    // Turn off the timeout on the tcp_stream, because
    // the websocket stream has its own timeout system.
    beast::get_lowest_layer(ws_).expires_never();

    // Set suggested timeout settings for the websocket
    ws_.set_option(
        websocket::stream_base::timeout::suggested(
            beast::role_type::client));

    // We need to set the User-Agent of the handshake. Beast's websocket
    // requires that this be done using a decorator.
    ws_.set_option(websocket::stream_base::decorator(
        [](websocket::request_type& req)
        {
            req.set(http::field::user_agent,
                std::string(BOOST_BEAST_VERSION_STRING) +
                " websocket-client-async-ssl");
        })
    );

    // The websocket should use deflate where possible, to reduce bandwidth
    // by compressing the messages on the wire.
    // This requires that zlib be included as a dependency.
    websocket::permessage_deflate opt;
    opt.client_enable = true; // for clients
    opt.server_enable = true; // for servers
    ws_.set_option(opt);

    // Perform the websocket handshake
    ws_.async_handshake(host_, "/",
        [this](error_code ec)
        {
            on_handshake(ec);
        }
    );
}

// This is the function that is called when the websocket is up and usable.
// The previous steps were relatively generic across all websocket connections,
// and from this point on we need to include business logic.
void live_price_listener::on_handshake(beast::error_code ec)
{
    if (ec) {
        cancel();
        return error_handler_(ec, "handshake");
    }

    // If we have been asked to shut down then do no further processing.
    if (!active_)
        return;

    // Construct a coinbase json subscription message, using Boost::json
    json::value jv = {
            { "type", "subscribe" },
            { "product_ids", json::array(coins_.cbegin(), coins_.cend()) },
            { "channels", json::array{
                "heartbeat",
                "ticker_batch" }
        }
    };

    // Convert the json object into a string.
    std::string subscribe_json_str = serialize(jv);

    // Send the subscription message to the server.
    ws_.async_write(
        net::buffer(subscribe_json_str),
        [this, len=subscribe_json_str.size()](error_code ec, std::size_t bytes_transferred)
        {
            on_write(ec, len, bytes_transferred);
        }
    );
}

void live_price_listener::on_write(
      beast::error_code ec
    , std::size_t bytes_required
    , std::size_t bytes_transferred)
{
    // Check for errors and verify that the byte count sent in the subscription message
    // is what we expected.
    if (ec) {
        cancel();
        return error_handler_(ec, "write");
    }
    else if (bytes_transferred < bytes_required) {
        cancel();
        // TODO: figure out what to put in ec here.
        return error_handler_(ec, "write");
    }

    // If we have been asked to shut down then do no further processing.
    if (!active_)
        return;

    // Read a message into our buffer. Note that buffer_ is a member variable as it has
    // to persist for the life of the read and until the on_read completion handler is
    // finished.
    ws_.async_read(
        buffer_,
        [this](error_code ec, std::size_t bytes_transferred)
        {
            on_read(ec, bytes_transferred);
        }
    );
}

void live_price_listener::on_read(beast::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    // This indicates that the session was closed
    if (ec == websocket::error::closed) {
        cancel();
        return;
    }
    else if (ec) {
        cancel();
        if (active_) return error_handler_(ec, "read");
    }

    // If we have been asked to shut down then do no further processing.
    if (!active_)
        return;

    // The asynchronous read performs its own commit() on the dynamic buffer, thus the readable
    // section of the dynamic buffer contains the message we want to decode.
    asio::const_buffer buf(buffer_.cdata());

    // We convert the const_buffer buf into a string_view, and then parse the json string itself.
    // Note that an alternative would be to use beast::buffers_to_string but that would
    // perform an additional allocation.
    parse_json(core::string_view(static_cast<const char*>(buf.data()), buf.size()));

    std::cout << "Interim: " << beast::make_printable(buffer_.data()) << "\n\n" << std::endl;

    //receive_handler_(beast::buffers_to_string(buffer_.data()));

    // Erase the const section of the dynamic buffer.
    // Note: the clear() function does not deallocate so the capactity of the flat_buffer is
    // unchanged, preventing the need for a reallocation each time a message is received.
    buffer_.clear();

    count++;
    if (count > 20) cancel();

    // This is a very common idiom in async programming. As soon as a read completes, we
    // initiate another asynchronous read, almost like an infinite loop.
    ws_.async_read(
        buffer_,
        [this](error_code ec, std::size_t bytes_transferred)
        {
            on_read(ec, bytes_transferred);
        }
    );
}



void live_price_listener::parse_json(core::string_view str)
{
    parser_.reset();

    boost::system::error_code ec;
    parser_.write(str, ec);

    if (ec)
        return error_handler_(ec, "json_price_decoder::parse_json");

    json::value jv(parser_.release());

    core::string_view productstr;
    core::string_view timestr;

    double price = 0;

    // Design note: the json parsing could be done without using the exception
    // interface, but the resulting code would be considerably more verbose.
    try {
        if (jv.as_object().at("type").as_string() != "ticker")
            return;

        productstr = jv.as_object().at("product_id").as_string();

        core::string_view pricestr = jv.as_object().at("price").as_string();

        timestr = jv.as_object().at("time").as_string();

        std::size_t str_size = 0;
        price = std::stod(pricestr, &str_size);
    }
    catch (boost::system::system_error se) {
        return error_handler_(ec, "json_price_decoder::parse_json parse failure");
    }
    catch (std::invalid_argument se) {
        return error_handler_(ec, "json_price_decoder::parse_json parse failure");
    }
    catch (std::out_of_range se) {
        return error_handler_(ec, "json_price_decoder::parse_json parse failure");
    }

    // As timestr is a *UTC* string, we want to generate a chrono::system_clock::time_point
    // representing the UTC time.
    posix_time::ptime ptime = posix_time::from_iso_extended_string(timestr);
    std::time_t epoch_time = posix_time::to_time_t(ptime);
    const auto price_time = std::chrono::system_clock::from_time_t(epoch_time);

    if (receive_handler_)
        receive_handler_(productstr, price_time, price);

    std::cout << "Decoded live " << productstr << " price: " << price << " at " << price_time << std::endl;

    //std::this_thread::sleep_for(std::chrono::milliseconds(10*1000));
}

void live_price_listener::on_close(beast::error_code ec)
{
    if (ec && ec != boost::asio::ssl::error::stream_truncated)
        return error_handler_(ec, "close");

    // If we get here then the connection is closed gracefully

    // The make_printable() function helps print a ConstBufferSequence
    std::cout << "Final buffer content:" << beast::make_printable(buffer_.data()) << std::endl;
}
