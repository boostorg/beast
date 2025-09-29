//
// Copyright (c) 2025 Mungo Gill
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance
//

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

#include "json_price_decoder.hpp"

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
fail(beast::error_code ec, char const* what)
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
    ssl::context ctx{ ssl::context::tlsv12_client };

    // Verify the remote server's certificate
    ctx.set_verify_mode(ssl::verify_peer);

    // This holds the root certificate used for verification
    load_root_certificates(ctx);

    net::thread_pool decoder_tp(1);

    auto decoded_recv = [](const std::string& symbol, double price) {
        std::cout << "Decoded Recv" << symbol << ":" << price << "\n" << std::endl;
    };

    json_price_decoder decoder_worker(decoder_tp, decoded_recv, fail);

    decoder_worker.run();

	auto live_input_recv = [&decoder_worker](std::string&& v) {
		//std::cout << "Live input Recv" << v << "\n" << std::endl;
        decoder_worker.post(processor_base::input_type::LIVE, std::move(v));
	};

    auto historic_input_recv = [&decoder_worker](std::string&& v) {
        std::cout << "Historic input Recv" << v << "\n" << std::endl;
        decoder_worker.post(processor_base::input_type::HISTORIC, std::move(v));
    };

    // The io_context is required for all I/O
    net::io_context listen_ioc;

    // Construct and start a the fetcher of historic prices.
    historic_price_fetcher historic_fetcher(listen_ioc, ctx, coins, historic_input_recv, fail);
    historic_fetcher.run();

    // Construct and start a the websocket listener.
    live_price_listener listen_worker(listen_ioc, ctx, coins, live_input_recv, fail);
    listen_worker.run();

    // Run the I/O service. The call will return when
    // the socket is closed.
    listen_ioc.run();

    decoder_tp.join();

    return EXIT_SUCCESS;
}
