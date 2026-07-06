//
// Copyright (c) 2025 Mungo Gill
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_EXAMPLE_LIVE_PRICE_LISTENER
#define BOOST_BEAST_EXAMPLE_LIVE_PRICE_LISTENER

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/json.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>

#include <iostream>
#include <string>

#include "processor_base.hpp"

namespace boost
{

// Opens a websocket and subsscribes to price ticks
template<class Executor>
class live_price_listener : public processor_base
{
    // This holds the function called when a live price is received.
    std::function<void(
        const std::string&,
        std::chrono::system_clock::time_point,
        double)>                                         receive_handler_;

    // This holds the function called when an error happens.
    std::function<void(system::error_code, char const*)> error_handler_;

    // Note that the websocket uses composed operations internally, as well as having internal
    // timers. Thus this executor needs to be an (implicit or explicit) strand, which 
    // guarantees that no two completion handlers will be run simultaneously (from differnt threads).
    Executor& exec_;

    // The resolver's role is to perform DNS lookups from a hostname to a set of ip addresses.
    asio::ip::tcp::resolver resolver_;

    // The key structure in this listener is the websocket itself.
    boost::beast::websocket::stream<asio::ssl::stream<boost::beast::tcp_stream>> ws_;

    // Any calls to async_read need to be passed a buffer into which the response will
    // be written. That buffer needs to persist until after the read completes and the
    // completion handler is called.
    boost::beast::flat_buffer buffer_;

    // The subscription message needs to persist until the asynchronous operation initiated
    // by ws_.async_write() completes. Thus it is held here as a member.
    std::string subscribe_json_str_;

    // The host will be used at multiple stages during the websocket's setup process.
    std::string host_;

    // A list of coins that we want to get the prices for.
    std::vector<std::string> coins_;

    // Provide a mechanism to exit the websocket subscription. When active_ is false, 
    // no more asynchronous calls will be made, and every completion handler called will exit
    // immediately.
    bool active_;

    // It is more efficient to persist the json parser so that memory allocation does not need
    // to be repeated each time we docode a message
    boost::json::parser parser_;

    int testing_count = 0;

public:
    // It is worth noting that we do not retain the reference to the passed-in io_context.
    // The reason for this is that we use a strand to ensure the 
    explicit
        live_price_listener(
            Executor& exec
            , asio::ssl::context& ctx
            , string_view host
            , const std::vector<std::string>& coins
            , std::function<void(
                const std::string&
                , std::chrono::system_clock::time_point
                , double)> receive_handler
            , std::function<void(system::error_code, char const*)> err_handler)
        : receive_handler_(receive_handler)
        , error_handler_(err_handler)
        , exec_(exec)
        , resolver_(exec_)
        , ws_(exec_, ctx)
        , host_(host)
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
            system::error_code ec,
            asio::ip::tcp::resolver::results_type results);

    void
        on_connect(
            system::error_code ec,
            asio::ip::tcp::resolver::results_type::endpoint_type ep);

    void
        on_ssl_handshake(system::error_code ec);

    void
        on_handshake(system::error_code ec);

    void
        on_write(
            system::error_code ec,
            std::size_t bytes_transferred);

    void
        on_read(
            system::error_code ec,
            std::size_t bytes_transferred);

    void parse_json(
        boost::core::string_view str);

    void
        on_close(system::error_code ec);
};

// Start the asynchronous operation
template<class Executor>
void live_price_listener<Executor>::run()
{


    // Set SNI Hostname (many hosts need this to handshake successfully)
    // Note that ws_.next_layer() references the asio::ssl::stream object.
    // Note, SSL_set_tlsext_host_name is an OpenSSL C function, not
    // part of asio or beast.
    if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host_.c_str()))
    {
        system::error_code ec{
            static_cast<int>(::ERR_get_error()),
            asio::error::get_ssl_category() };
        return error_handler_(ec, "SNI");
    }

    // Set the expected hostname in the peer certificate for verification.
    // OpenSSL will, whenever a certificate is received, call this function
    // to check that the certificate's host matches what we think it should be.
    ws_.next_layer().set_verify_callback(asio::ssl::host_name_verification(host_));

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
        [this](system::error_code ec, asio::ip::tcp::resolver::results_type results)
        {
            // Note that `results` is actually an iterator into a container of
            // endpoints representing all the IP addresses found by the DNS lookup.
            on_resolve(ec, results);
        }
    );
}

template<class Executor>
void live_price_listener<Executor>::cancel()
{
    // We set active_=false to rapidly consume all the pending
    // completion handlers.
    active_ = false;

    // We use asio::post to ensure the websocket closure takes place after
    // all the currently pending completion handlers.
    asio::post(exec_, [this]()
        {
            // If the websocket is still open, close it.
            if (ws_.is_open())
                ws_.async_close(beast::websocket::close_code::normal,
                    [this](system::error_code ec)
                    {
                        on_close(ec);
                    }
                );
        }
    );
}

// This is the function called when hostname resolution completes.
template<class Executor>
void live_price_listener<Executor>::on_resolve(system::error_code ec, asio::ip::tcp::resolver::results_type results)
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
        [this](system::error_code ec, asio::ip::tcp::resolver::results_type::endpoint_type ep)
        {
            // Note that the endpoint `ep` represents the single IP to which
            // we successfully connected (if any).
            on_connect(ec, ep);
        }
    );
}

// Once the underlying socket is connected, this function performs the next step,
// namely getting the SSL layer running.
template<class Executor>
void live_price_listener<Executor>::on_connect(system::error_code ec, asio::ip::tcp::resolver::results_type::endpoint_type ep)
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
        asio::ssl::stream_base::client,
        [this](system::error_code ec)
        {
            on_ssl_handshake(ec);
        }
    );
}

template<class Executor>
void live_price_listener<Executor>::on_ssl_handshake(system::error_code ec)
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
        beast::websocket::stream_base::timeout::suggested(
            beast::role_type::client));

    // We need to set the User-Agent of the handshake. Beast's websocket
    // requires that this be done using a decorator.
    ws_.set_option(beast::websocket::stream_base::decorator(
        [](beast::websocket::request_type& req)
        {
            req.set(beast::http::field::user_agent,
                std::string(BOOST_BEAST_VERSION_STRING) +
                " websocket-client-async-ssl");
        })
    );

    // The websocket should use deflate where possible, to reduce bandwidth
    // by compressing the messages on the wire.
    // This requires that zlib be included as a dependency.
    beast::websocket::permessage_deflate opt;
    opt.client_enable = true; // for clients
    opt.server_enable = true; // for servers
    ws_.set_option(opt);

    // Perform the websocket handshake
    ws_.async_handshake(host_, "/",
        [this](system::error_code ec)
        {
            on_handshake(ec);
        }
    );
}

// This is the function that is called when the websocket is up and usable.
// The previous steps were relatively generic across all websocket connections,
// and from this point on we need to include business logic.
template<class Executor>
void live_price_listener<Executor>::on_handshake(system::error_code ec)
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
    subscribe_json_str_ = serialize(jv);

    // Send the subscription message to the server.
    ws_.async_write(
        asio::buffer(subscribe_json_str_),
        [this](system::error_code ec, std::size_t bytes_transferred)
        {
            on_write(ec, bytes_transferred);
        }
    );
}

template<class Executor>
void live_price_listener<Executor>::on_write(
    system::error_code ec
    , std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    // Check for errors.
    if (ec) {
        cancel();
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
        [this](system::error_code ec, std::size_t bytes_transferred)
        {
            on_read(ec, bytes_transferred);
        }
    );
}

template<class Executor>
void live_price_listener<Executor>::on_read(system::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    // This indicates that the session was closed
    if (ec == beast::websocket::error::closed) {
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

    testing_count++;
    if (testing_count > 20) cancel();

    // This is a very common idiom in async programming. As soon as a read completes, we
    // initiate another asynchronous read, almost like an infinite loop.
    ws_.async_read(
        buffer_,
        [this](system::error_code ec, std::size_t bytes_transferred)
        {
            on_read(ec, bytes_transferred);
        }
    );
}



template<class Executor>
void live_price_listener<Executor>::parse_json(core::string_view str)
{
    parser_.reset();

    system::error_code ec;
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
    catch (system::system_error se) {
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

template<class Executor>
void live_price_listener<Executor>::on_close(system::error_code ec)
{
    if (ec && ec != asio::ssl::error::stream_truncated)
        return error_handler_(ec, "close");

    // If we get here then the connection is closed gracefully

    // The make_printable() function helps print a ConstBufferSequence
    std::cout << "Final buffer content:" << beast::make_printable(buffer_.data()) << std::endl;
}

}

#endif
