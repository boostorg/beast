//
// Copyright (c) 2025 Mungo Gill
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#if 1

#include "example/common/root_certificates.hpp"

#include "historic_price_fetcher.hpp"
#include "live_price_listener.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/tokenizer.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>

#include "price_store.hpp"

#include <iostream>
#include <functional>


using namespace boost;
using namespace std::placeholders;

using namespace beast;         // from <boost/beast.hpp>
using namespace http;           // from <boost/beast/http.hpp>
using namespace websocket; // from <boost/beast/websocket.hpp>

namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

// Report a failure
void
fail(system::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

//------------------------------------------------------------------------------

int main(int argc, char** argv)
{
    // Check command line arguments.
    if (argc != 2 && argc != 3)
    {
        std::cerr <<
            "Usage: " << *argv << " <coin-list> [<openrouter.ai-api-key>] \n" <<
            "Example:\n" <<
            "    " << *argv << " 'BTC-USD,ETH-USD' \n" <<
            "    " << *argv << " 'BTC-USD,ETH-USD' ABC-DEF-GHI-JKL\n";
        return EXIT_FAILURE;
    }

    std::string coin_list_str(argv[1]);
    std::vector<std::string> coins;

    char_separator<char> separator(", ");
    tokenizer<char_separator<char>> tokens(coin_list_str, separator);
    for (auto it = tokens.begin(); it != tokens.end(); it++) {
        coins.push_back(*it);
    };

    // The SSL context is required, and holds certificates
    ssl::context ssl_ctx{ ssl::context::tlsv12_client };

    // Verify the remote server's certificate
    ssl_ctx.set_verify_mode(ssl::verify_peer);

    // This holds the root certificate used for verification
    load_root_certificates(ssl_ctx);

    auto decoded_recv = [](const std::string& symbol, double price) {
        std::cout << "Decoded Recv" << symbol << ":" << price << "\n" << std::endl;
    };

    auto price_store_update_recv = [](const std::string&) {

    };

    price_store store(coins, price_store_update_recv);

	auto live_input_recv = [&store](const std::string& coin,
		std::chrono::system_clock::time_point time,
		double price) {
			store.post(coin, time, price);
		};

    auto historic_input_recv = [&store](const std::string& coin,
        std::chrono::system_clock::time_point time,
        double price) {
        store.post(coin, time, price);
    };

    // The io_context is required for all I/O
    net::io_context listen_ioc;

    // Construct and start a the fetcher of historic prices.
    asio::strand<asio::io_context::executor_type> strand = asio::make_strand(listen_ioc);

    std::string host = "api.coinbase.com";

    auto fetcher = boost::historic_fetcher(
        strand,
        ssl_ctx,
        host,
        historic_input_recv);

    fetcher.async_historic_fetch(
        coins,
        [](boost::system::error_code ec)
        {
            if (ec.failed())
            {
                fail(ec, "async_historic_fetch");
            }
        });
    //historic_price_fetcher historic_fetcher(listen_ioc, ssl_ctx, coins, historic_input_recv, fail);
    //historic_fetcher.run();

    // Now we run the event loop until all historic prices have been received.
    listen_ioc.run();

    // Skip the live stuff until we get the changes to historic pricing working.
    //return EXIT_SUCCESS;

    // Construct and start a the websocket listener.
    // For this example hard-code the host.
    //host_ = "ws-feed-public.sandbox.exchange.coinbase.com";
    std::string ws_host = "ws-feed.exchange.coinbase.com";
    live_price_listener listen_worker(listen_ioc, ssl_ctx, ws_host, coins, live_input_recv, fail);
    listen_worker.run();

    // Restart the event loop. The run() call will return when
    // the socket is closed.
    listen_ioc.restart();
    listen_ioc.run();

    return EXIT_SUCCESS;
}

#endif

#if 0
// Type your code here, or load an example.
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
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

namespace boost {

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

        using resolver_type = asio::ip::basic_resolver<
            asio::ip::tcp, Executor>;

        using tcp_stream_type = beast::basic_stream<
            asio::ip::tcp,
            Executor>;

        using ssl_stream_type = boost::asio::ssl::stream<tcp_stream_type>;

        std::unique_ptr<resolver_type> resolver_p_;
        std::unique_ptr<ssl_stream_type> ssl_stream_p_;

        std::string host_;
        state state_;

    public:
        explicit
            historic_fetcher_op(
                Executor& exec
                , boost::asio::ssl::context& ctx)
            : state_(state::starting)
        {
            // For this example use a hard-coded host name.
            // In reality this would be stored in some form of configuration.
            //host_ = "api.coinbase.com";
            host_ = "www.boost.org";
            //host_ = "ws-feed.exchange.coinbase.com";

            resolver_p_ = std::make_unique<resolver_type>(exec);
            ssl_stream_p_ = std::make_unique<ssl_stream_type>(exec, ctx);

            // Set the expected hostname in the peer certificate for verification

            ssl_stream_p_->set_verify_callback(boost::asio::ssl::host_name_verification(host_));
        }

        template <typename Self>
        void operator()(
            Self& self
            , system::error_code ec = {})
        {
            if (ec) {
                state_ = state::error;
                return self.complete(ec);
            }

            switch (state_) {
            case state::starting: {
                // Set SNI Hostname (many hosts need this to handshake successfully)
                if (!SSL_set_tlsext_host_name(ssl_stream_p_->native_handle(), host_.c_str()))
                {
                    system::error_code ssl_ec{
                        static_cast<int>(::ERR_get_error()),
                        asio::error::get_ssl_category() };
                    state_ = state::error;
                    return self.complete(ssl_ec);
                }

                // Look up the domain name
                state_ = state::resolving;
                resolver_p_->async_resolve(
                    host_,
                    "443",
                    std::move(self)
                );
            } break;
            case state::ssl_handshaking: {
                // TODO: If there are any coins left to request, request one.
            } break;
            default: {
                // This should not happen.
                throw std::logic_error("unreachable");
            }
            }
        };

        template <typename Self>
        void operator()(
            Self& self
            , system::error_code ec
            , asio::ip::tcp::resolver::results_type results)
        {
            if (ec) {
                state_ = state::error;
                return self.complete(ec);
            }

            switch (state_) {
            case state::resolving: {
                // Set a timeout on the operation
                beast::get_lowest_layer(*ssl_stream_p_).expires_after(std::chrono::seconds(30));

                // Make the connection on the IP address we get from a lookup
                state_ = state::connecting;
                for (auto& r : results) {
                    std::cout << r.endpoint() << std::endl;
                }
                beast::get_lowest_layer(*ssl_stream_p_).async_connect(
                    results,
                    std::move(self)
                );
                std::cout << "async_connect has been called" << std::endl;
            } break;
            default: {
                // This should not happen.
                throw std::logic_error("unreachable");
            }
            }
        };

        template <typename Self>
        void operator()(
            Self& self
            , system::error_code ec
            , asio::ip::tcp::resolver::results_type::endpoint_type ep)
        {
            boost::ignore_unused(ep);

            if (ec) {
                std::cout << "Connection failed" << std::endl;
                state_ = state::error;
                return self.complete(ec);
            }

            switch (state_) {
            case state::connecting: {
                // Set a timeout on the operation
                std::cout << "Connection succeeded" << std::endl;

                // This is where the ssl handshaking stuff will go if we
                // can work out why the connection is failing.

                self.complete(ec);
            } break;
            default: {
                // This should not happen.
                throw std::logic_error("unreachable");
            }
            }
        };

    };

    template<
        class Executor,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(
            system::error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(
        system::error_code))
        async_historic_fetch(
            Executor& exec
            , boost::asio::ssl::context& ssl_ctx
            , CompletionToken&& token)
    {
        return asio::async_compose<
            CompletionToken,
            void(system::error_code)>(
                historic_fetcher_op<Executor>(
                    exec
                    , ssl_ctx)
                , token
                , exec);
    }

} // end namespace boost

using namespace boost;

int main()
{
    // The SSL context is required, and holds certificates
    asio::ssl::context ssl_ctx{ asio::ssl::context::tlsv12_client };

    // Verify the remote server's certificate
    ssl_ctx.set_verify_mode(asio::ssl::verify_peer);

    // The io_context is required for all I/O
    asio::io_context listen_ioc;

    // Construct and start a the fetcher of historic prices.
    //asio::strand<asio::io_context::executor_type> strand = asio::make_strand(listen_ioc);
    auto executor = listen_ioc.get_executor();
    boost::async_historic_fetch(
        executor,
        ssl_ctx,
        [](boost::system::error_code ec)
        {
            if (ec.failed())
            {
                std::cerr << ec.message() << "\n";
            }
        });

    // Now we run the event loop until all historic prices have been received.
    listen_ioc.run();
}
#endif
