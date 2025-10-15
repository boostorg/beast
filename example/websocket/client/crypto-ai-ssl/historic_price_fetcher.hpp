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
        enum class state {
            starting,
            resolving,
            connecting,
            ssl_handshaking,
            writing,
            reading,
            error,
            complete
        };

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

        client_type&     client_;
        resolver_type&   resolver_;
        ssl_stream_type& ssl_stream_;

        buffer_type&   buffer_;
        request_type&  request_;
        response_type& response_;

    public:

        explicit
            historic_fetcher_op(
                client_type& client
                , resolver_type& resolver
                , ssl_stream_type& ssl_stream
                , buffer_type& buffer
                , request_type& request
                , response_type& response)
            : client_(client)
            , resolver_(resolver)
            , ssl_stream_(ssl_stream)
            , buffer_(buffer)
            , request_(request)
            , response_(response)
        {
        }

        template <typename Self>
        void operator()(
            Self& self)
        {
            // Look up the domain name
            resolver_.async_resolve(
                client_.get_host(),
                "https",
                asio::prepend(std::move(self), on_resolve{})
            );
        };

        template <typename Self>
        void operator()(
            Self& self
            , on_ssl_handshake
            , system::error_code ec)
        {
            if (ec) {
                return self.complete(ec);
            }

            // Set up an HTTP GET request message
            request_.version(11);
            request_.method(beast::http::verb::get);
            request_.set(beast::http::field::host, client_.get_host());
            request_.set(beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
            
            // If there are any coins left to request, request one.
            return send_next_request(self);
        };

        template <typename Self>
        void operator()(
            Self& self
            , on_resolve
            , system::error_code ec
            , asio::ip::tcp::resolver::results_type results)
        {
            if (ec) {
                return self.complete(ec);
            }

            // Set a timeout on the operation
            beast::get_lowest_layer(ssl_stream_).expires_after(std::chrono::seconds(30));

            // Make the connection on the IP address we get from a lookup
            beast::get_lowest_layer(ssl_stream_).async_connect(
                results,
                asio::prepend(std::move(self), on_connect{})
            );

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
                return self.complete(ec);
            }

            // Set the expected hostname in the peer certificate for verification
            ssl_stream_.set_verify_callback(boost::asio::ssl::host_name_verification(client_.get_host()));

            // Set SNI Hostname (many hosts need this to handshake successfully)
            if (!SSL_set_tlsext_host_name(ssl_stream_.native_handle(), client_.get_host()))
            {
                system::error_code ssl_ec{
                    static_cast<int>(::ERR_get_error()),
                    asio::error::get_ssl_category() };
                return self.complete(ssl_ec);
            }

            // Set a timeout on the operation
            beast::get_lowest_layer(ssl_stream_).expires_after(std::chrono::seconds(30));

            // Perform the SSL handshake
            ssl_stream_.async_handshake(
                boost::asio::ssl::stream_base::client,
                asio::prepend(std::move(self), on_ssl_handshake{})
            );
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
                return self.complete(ec);
            }

            // Read a message into our buffer
            beast::http::async_read(
                ssl_stream_,
                buffer_,
                response_,
                asio::prepend(std::move(self), on_read_result{})
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
                return self.complete(ok);
            }
            else if (ec) {
                return self.complete(ec);
            }

            // Process the received message

            // Write the message to standard out
            //std::cout << "Response: " << response_ << "\n" << std::endl;
            std::cout << "Body: " << response_.body() << "\n\n" << std::endl;

            // The response can be quite long, and we can avoid an allocation
            // by swapping the body into an empty string.
            std::string temp;
            temp.swap(response_.body());

            client_.process_response(temp, ec);
            if (ec) {
                return self.complete(ec);
            }

            send_next_request(self);

        };

        template <typename Self>
        void send_next_request(Self& self)
        {
            if (!client_.requests_outstanding()) {
                system::error_code ok{};
                return self.complete(ok);
            }

            // Set up an HTTP GET request message
            request_.target(client_.next_request());

            // Set a timeout on the operation
            beast::get_lowest_layer(ssl_stream_).expires_after(std::chrono::seconds(30));

            // Send the message
            beast::http::async_write(
                ssl_stream_,
                request_,
                asio::prepend(std::move(self), on_write_request{})
            );
        }
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
        std::time_t start_of_day_;
        std::string host_;

        std::string current_coin_;

        // It is more efficient to persist the json parser so that memory allocation does not need
        // to be repeated each time we docode a message
        json::parser parser_;

    public:
        //using socket_type =
        //    asio::basic_socket<asio::ip::tcp, Executor>;

        //using endpoint_type = asio::ip::tcp::endpoint;

        explicit
            historic_fetcher(
                Executor& exec
                , boost::asio::ssl::context& ctx
                , const std::vector<std::string>& coins
                , std::function<void(
                    const std::string&
                    , std::chrono::system_clock::time_point
                    , double)> receive_handler)
            : receive_handler_(receive_handler)
            , resolver_(exec)
            , ssl_stream_(exec, ctx)
            , coins_(coins)
            , start_of_day_(0)
        {
            // For this example use a hard-coded host name.
            // In reality this would be stored in some form of configuration.
            host_ = "api.coinbase.com";
            //host_ = "www.boost.org";
            //host_ = "ws-feed.exchange.coinbase.com";

            // Set the expected hostname in the peer certificate for verification
            ssl_stream_.set_verify_callback(boost::asio::ssl::host_name_verification(host_));

            // Find the subscription start time
            posix_time::ptime ptime = posix_time::second_clock::universal_time();
            posix_time::ptime sod = posix_time::ptime(ptime.date());
            start_of_day_ = posix_time::to_time_t(sod);
        }

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
            boost::core::string_view str, boost::system::error_code& ec)
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
                    if (start_time > start_of_day_) {
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

        template<
            class Executor,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(
                system::error_code)) CompletionToken>
        BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(
            system::error_code))
            async_historic_fetch(
                Executor& exec
                , CompletionToken&& token)
        {
            return asio::async_compose<
                CompletionToken,
                void(system::error_code)>(
                    historic_fetcher_op<Executor>(
                        *this
                        , resolver_
                        , ssl_stream_
                        , buffer_
                        , request_
                        , response_)
                    , token
                    , exec);
        }
    };


    // The old non-executor version temporarily pasted below.
#if 0
//using namespace boost;

// Opens a websocket and subsscribes to price ticks
    class historic_price_fetcher : public processor_base
    {
        std::function<void(
            const std::string&,
            std::chrono::system_clock::time_point,
            double)>                                               receive_handler_;
        std::function<void(boost::system::error_code, char const*)> error_handler_;

        boost::asio::strand<boost::asio::io_context::executor_type> strand_;
        boost::asio::ip::tcp::resolver resolver_;
        boost::asio::ssl::stream<boost::beast::tcp_stream> stream_;

        boost::beast::flat_buffer buffer_;
        boost::beast::http::request<boost::beast::http::string_body> req_;
        boost::beast::http::response<boost::beast::http::string_body> response_;

        std::string host_;
        std::vector<std::string> coins_;

        std::string current_coin_;

        std::time_t start_of_day_;
        bool active_;

        // It is more efficient to persist the json parser so that memory allocation does not need
        // to be repeated each time we docode a message
        boost::json::parser parser_;

    public:
        // Resolver and socket require an io_context
        explicit
            historic_price_fetcher(
                boost::asio::io_context& ioc
                , boost::asio::ssl::context& ctx
                , const std::vector<std::string>& coins
                , std::function<void(
                    const std::string&
                    , std::chrono::system_clock::time_point
                    , double)> receive_handler
                , std::function<void(boost::system::error_code, char const*)> err_handler)
            : receive_handler_(receive_handler)
            , error_handler_(err_handler)
            , strand_(boost::asio::make_strand(ioc))
            , resolver_(strand_)
            , stream_(strand_, ctx)
            , coins_(coins)
            , start_of_day_(0)
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
                boost::system::error_code ec,
                boost::asio::ip::tcp::resolver::results_type results);

        void
            on_connect(
                boost::system::error_code ec,
                boost::asio::ip::tcp::resolver::results_type::endpoint_type ep);

        void
            on_ssl_handshake(boost::system::error_code ec);

        void
            next_request();

        void
            on_write(
                boost::system::error_code ec,
                std::size_t bytes_transferred);

        void
            on_read(
                boost::system::error_code ec,
                std::size_t bytes_transferred);

        void parse_json(
            boost::core::string_view str);

        void
            on_shutdown(boost::system::error_code ec);
    };
#endif

} // end namespace boost

#endif

