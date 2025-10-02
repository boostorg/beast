//
// Copyright (c) 2025 Mungo Gill
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance
//

#ifndef BOOST_BEAST_EXAMPLE_JSON_PRICE_DECODER_H
#define BOOST_BEAST_EXAMPLE_JSON_PRICE_DECODER_H

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/json.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>

#include <iostream>
#include <functional>
#include <deque>

#include "processor_base.hpp"

using namespace boost;
using namespace std::placeholders;

using namespace beast;         // from <boost/beast.hpp>
using namespace http;           // from <boost/beast/http.hpp>
using namespace websocket; // from <boost/beast/websocket.hpp>
using namespace json;

namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

// Opens a websocket and subsscribes to price ticks
class json_price_decoder : public processor_base
{
	std::function<void(const std::string&, double)> receive_handler_;
	std::function<void(beast::error_code, char const*)> error_handler_;
    net::execution_context &ctx_;
    bool active_;

public:
    // Resolver and socket require an io_context
    explicit
        json_price_decoder(net::execution_context& ec
            , std::function<void(const std::string&, double)> receive_handler
            , std::function<void(beast::error_code, char const*)> err_handler)
        : receive_handler_(receive_handler)
        , error_handler_(err_handler)
        , ctx_(ec)
        , active_(false)
    {
    }

    // Start the asynchronous operation
    void
        run()
    {
        active_ = true;
    }

    void
        post(input_type type, std::string &&str)
    {
        if (!active_)
            return;

        net::post(net::get_associated_executor(ctx_), [this, type, s = std::move(str)] () mutable
            {
                on_process(type, std::move(s));
            }
        );
    }

    void
        cancel() override
    {
        active_ = false;
    }

private:

    inline std::time_t my_time_gm(struct tm* tm) {
#if defined(_DEFAULT_SOURCE) // Feature test for glibc
        return timegm(tm);
#elif defined(_MSC_VER) // Test for Microsoft C/C++
        return _mkgmtime(tm);
#else
#error "Neither timegm nor _mkgmtime available"
#endif
    }

	void
		on_process(
              input_type type
			, std::string &&str)
	{
        if (!active_)
            return;

        boost::system::error_code ec;
        monotonic_resource mr;
        const value jv = parse(str, ec, &mr);

		if (ec)
			return error_handler_(ec, "json_price_decoder::on_process");

        if (type == input_type::LIVE)
        {
            try {
                if (jv.as_object().at("type").as_string() != "ticker")
                    return;

                json::string_view productstr = jv.as_object().at("product_id").as_string();

                json::string_view pricestr = jv.as_object().at("price").as_string();

                std::size_t str_size = 0;
                double price = std::stod(pricestr, &str_size);

                json::string_view timestr = jv.as_object().at("time").as_string();

                std::tm t = {}; // tm_isdst = 0
                std::istringstream ss(timestr);
                ss.imbue(std::locale()); // "LANG=C"

                ss >> std::get_time(&t, "%Y-%m-%dT%H:%M:%S.");
                if (ss.fail())
                    return error_handler_(ec, "json_price_decoder::on_process parse failure");

                // fix up the day of week, day of year etc
                t.tm_isdst = 0;
                t.tm_wday = -1; // a canary for a my_time_gm error

                std::time_t epoch_time = my_time_gm(&t);

                if (epoch_time == -1 || t.tm_wday == -1) // "real error"
                    return error_handler_(ec, "json_price_decoder::on_process parse failure");

                const auto price_time = std::chrono::system_clock::from_time_t(epoch_time);

                std::cout << "Decoded live " << productstr << " price: " << price << " at " << price_time << std::endl;

            }
            catch (boost::system::system_error se) {
                return error_handler_(ec, "json_price_decoder::on_process parse failure");
            }
            catch (std::invalid_argument se) {
                return error_handler_(ec, "json_price_decoder::on_process parse failure");
            }
            catch (std::out_of_range se) {
                return error_handler_(ec, "json_price_decoder::on_process parse failure");
            }
        }
        if (type == input_type::HISTORIC)
        {
            std::cerr << "historic decoding not yet written" << std::endl;
        }
	}
};

#endif

