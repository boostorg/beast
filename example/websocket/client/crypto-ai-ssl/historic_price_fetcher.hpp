//
// Copyright (c) 2025 Mungo Gill
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_EXAMPLE_HISTORIC_PRICE_FETCHER
#define BOOST_BEAST_EXAMPLE_HISTORIC_PRICE_FETCHER

#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/json.hpp>
#include <boost/url/format.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>

#include <cstdlib>
#include <functional>
#include <string>

#include "processor_base.hpp"

namespace boost {

template<class Executor>
class historic_fetcher;

template<class Executor>
class historic_fetcher_op
{
    struct on_resolve {};
    struct on_connect {};
    struct on_ssl_handshake {};
    struct on_write_request {};
    struct on_read_result {};

    using resolver_type = asio::ip::basic_resolver<
        asio::ip::tcp, Executor>;

    using tcp_stream_type = beast::basic_stream<
        asio::ip::tcp,
        Executor>;

    using ssl_stream_type = boost::asio::ssl::stream<tcp_stream_type>;

    using client_type = historic_fetcher<Executor>;

    using buffer_type = boost::beast::flat_buffer;
    using request_type = beast::http::request<boost::beast::http::string_body>;
    using response_type = beast::http::response<boost::beast::http::string_body>;

public:

    explicit
        historic_fetcher_op(
            client_type& client)
        : client_(client)
        , start_of_day_(0)
    {
    }

    template <typename Self>
    void operator()(
        Self& self)
    {
        // Look up the domain name
        client_.resolver_.async_resolve(
            client_.get_host(),
            "https",
            asio::cancel_after(
                std::chrono::seconds(30),
                asio::prepend(std::move(self), on_resolve{}))
        );
    };


    template <typename Self>
    void operator()(
        Self& self
        , on_resolve
        , system::error_code ec
        , asio::ip::tcp::resolver::results_type results)
    {
        if (ec) {
            return do_complete(self, ec);
        }

        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(client_.ssl_stream_).async_connect(
            results,
            asio::cancel_after(
                std::chrono::seconds(30),
                asio::prepend(std::move(self), on_connect{})));

    };



    template <typename Self>
    void operator()(
        Self& self
        , on_connect
        , system::error_code ec
        , asio::ip::tcp::resolver::results_type::endpoint_type ep)
    {
        boost::ignore_unused(ep);

        if (ec) {
            return do_complete(self, ec);
        }

        // Set the expected hostname in the peer certificate for verification
        client_.ssl_stream_.set_verify_callback(boost::asio::ssl::host_name_verification(client_.get_host()));

        // Set SNI Hostname (many hosts need this to handshake successfully)
        if (!SSL_set_tlsext_host_name(client_.ssl_stream_.native_handle(), client_.get_host()))
        {
            system::error_code ssl_ec{
                static_cast<int>(::ERR_get_error()),
                asio::error::get_ssl_category() };
            return do_complete(self, ssl_ec);
        }

        // Perform the SSL handshake
        client_.ssl_stream_.async_handshake(
            boost::asio::ssl::stream_base::client,
            asio::cancel_after(
                std::chrono::seconds(30),
                asio::prepend(std::move(self), on_ssl_handshake{}))
        );
    };


    template <typename Self>
    void operator()(
        Self& self
        , on_ssl_handshake
        , system::error_code ec)
    {
        if (ec) {
            return do_complete(self, ec);
        }

        // Find the subscription start time
        posix_time::ptime ptime = posix_time::second_clock::universal_time();
        posix_time::ptime sod = posix_time::ptime(ptime.date());
        start_of_day_ = posix_time::to_time_t(sod);

        // Set up an HTTP GET request message
        client_.request_.version(11);
        client_.request_.method(beast::http::verb::get);
        client_.request_.set(beast::http::field::host, client_.get_host());
        client_.request_.set(beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        // If there are any coins left to request, request one.
        return send_next_request(self);
    };

    template <typename Self>
    void operator()(
        Self& self
        , on_write_request
        , system::error_code ec
        , std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec) {
            return do_complete(self, ec);
        }

        // Read a message into our buffer
        beast::http::async_read(
            client_.ssl_stream_,
            client_.buffer_,
            client_.response_,
            asio::cancel_after(
                std::chrono::seconds(30),
                asio::prepend(std::move(self), on_read_result{}))
        );
    };

    template <typename Self>
    void operator()(
        Self& self
        , on_read_result
        , system::error_code ec
        , std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec == beast::http::error::end_of_stream) {
            system::error_code ok{};
            return do_complete(self, ok);
        }
        else if (ec) {
            return do_complete(self, ec);
        }

        // Process the received message

        // Write the message to standard out
        //std::cout << "Response: " << response_ << "\n" << std::endl;
        std::cout << "Body: " << client_.response_.body() << "\n\n" << std::endl;

        // The response can be quite long, and we can avoid an allocation
        // by swapping the body into an empty string.
        std::string temp;
        temp.swap(client_.response_.body());

        client_.process_response(temp, start_of_day_, ec);
        if (ec) {
            return do_complete(self, ec);
        }

        send_next_request(self);

    };

    template <typename Self>
    void do_complete(Self& self, system::error_code ec)
    {
        self.complete(ec);
        client_.running_.clear();
        return;
    }

    template <typename Self>
    void send_next_request(Self& self)
    {
        if (!client_.requests_outstanding()) {
            system::error_code ok{};
            return do_complete(self, ok);
        }

        // Set up an HTTP GET request message
        client_.request_.target(client_.next_request());

        // Send the message
        beast::http::async_write(
            client_.ssl_stream_,
            client_.request_,
            asio::cancel_after(
                std::chrono::seconds(30),
                asio::prepend(std::move(self), on_write_request{}))
        );
    }

private:
    client_type& client_;
    std::time_t start_of_day_;
};

template<class Executor>
class historic_fetcher
{
    using resolver_type = asio::ip::basic_resolver<
        asio::ip::tcp, Executor>;

    using tcp_stream_type = beast::basic_stream<
        asio::ip::tcp,
        Executor>;

    using ssl_stream_type = boost::asio::ssl::stream<tcp_stream_type>;

    using buffer_type = boost::beast::flat_buffer;
    using request_type = beast::http::request<boost::beast::http::string_body>;
    using response_type = beast::http::response<boost::beast::http::string_body>;

    friend class historic_fetcher_op<Executor>;

public:

    explicit
        historic_fetcher(
            Executor& exec
            , boost::asio::ssl::context& ctx
            , const boost::string_view host
            , std::function<void(
                const std::string&
                , std::chrono::system_clock::time_point
                , double)> receive_handler)
        : receive_handler_(receive_handler)
        , resolver_(exec)
        , ssl_stream_(exec, ctx)
        , host_(host)
    {
        running_.clear();
    }

    template<
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(
            system::error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(
        system::error_code))
        async_historic_fetch(
            const std::vector<std::string>& coins,
            CompletionToken&& token)
    {
        coins_ = coins;

        bool already_running = running_.test_and_set();
        BOOST_ASSERT(!already_running);

        return asio::async_compose<
            CompletionToken,
            void(system::error_code)>(
                historic_fetcher_op<Executor>(
                    *this)
                , token
                , ssl_stream_);
    }

private:
    const char* get_host() {
        return host_.c_str();
    }

    bool requests_outstanding()
    {
        return coins_.size() != 0;
    }

    std::string next_request()
    {
        current_coin_ = coins_.back();
        coins_.pop_back();

        urls::url url =
            urls::format(
                "{}://api.coinbase.com/api/v3/brokerage/market/products/{}/candles",
                "https",
                current_coin_);

        // Data
        url.params().append({ "granularity", "ONE_MINUTE" });
        //url.params().append({ "start", std::to_string(start_of_day_) });
        url.params().append({ "limit", std::to_string(5) });

        return url.buffer();
    }

    void process_response(
        boost::core::string_view str, std::time_t start_of_day, boost::system::error_code& ec)
    {
        parser_.reset();

        parser_.write(str, ec);

        if (ec) return;

        json::value jv(parser_.release());

        // Design note: the json parsing could be done without using the exception
        // interface, but the resulting code would be considerably more verbose.
        try {
            auto candle_list = jv.as_object().at("candles").as_array();

            if (candle_list.size() == 0) {
                ec = json::make_error_code(boost::json::error::size_mismatch);
                return;
            }

            // The coinbase API provides the most recent values first.
            for (auto it = candle_list.crbegin(); it != candle_list.crend(); it++) {
                core::string_view startstr = it->as_object().at("start").as_string();
                core::string_view openstr = it->as_object().at("open").as_string();
                core::string_view closestr = it->as_object().at("close").as_string();

                std::size_t str_size = 0;
                std::time_t start_time = static_cast<std::time_t>(std::stoll(startstr, &str_size));
                if (start_time > start_of_day) {
                    double open = std::stod(openstr, &str_size);

                    if (receive_handler_)
                        receive_handler_(current_coin_, std::chrono::system_clock::from_time_t(start_time), open);

                    std::cout << "Decoded historic " << current_coin_ << " price: " << open << " at " << std::chrono::system_clock::from_time_t(start_time) << std::endl;
                }
            }
        }
        catch (boost::system::system_error se) {
            ec = se.code();
        }
        catch (std::invalid_argument se) {
            ec = json::make_error_code(boost::json::error::incomplete);
        }
        catch (std::out_of_range se) {
            ec = json::make_error_code(boost::json::error::out_of_range);
        }
    }

    std::function<void(
        const std::string&,
        std::chrono::system_clock::time_point,
        double)>                                                receive_handler_;

    resolver_type   resolver_;
    ssl_stream_type ssl_stream_;

    buffer_type   buffer_;
    request_type  request_;
    response_type response_;

    std::vector<std::string> coins_;
    std::string host_;
    std::atomic_flag running_;

    std::string current_coin_;

    // It is more efficient to persist the json parser so that memory allocation does not need
    // to be repeated each time we docode a message
    json::parser parser_;
};

} // end namespace boost

#endif

