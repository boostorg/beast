//
// Copyright (c) 2025 Mungo Gill
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_EXAMPLE_PRICE_STORE_H
#define BOOST_BEAST_EXAMPLE_PRICE_STORE_H

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include <chrono>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <mutex>


// Opens a websocket and subsscribes to price ticks
class price_store
{
	std::function<void(const std::string&)> update_handler_;

public:
    struct price_entry {
        std::chrono::system_clock::time_point time_;
        double price_;
    };

private:
	// As we wish to support one thread posting prices and one thread reading prices,
	// without having a thread-safe vector and without copying the entire vector each
	// time, and given we know we have a limited volume of data, we can adopt a
	// "double-buffer" technique (and accept the memory hit).
	std::map<
		std::string,
		std::pair<std::vector<price_entry>,
		          std::vector<price_entry> > > entries_;

	std::mutex mutex_;

public:
	explicit
		price_store(
			  const std::vector<std::string>& coins
			, std::function<void(const std::string&)> update_handler
		)
		: update_handler_(update_handler)
    {
		const std::lock_guard<std::mutex> guard(mutex_);
        // Prepopulate the map.
        // Note: this could, as an alternative design, be done "lazily"
		// as prices come in.
        for (auto& s : coins) {
			auto rv = entries_.emplace(std::piecewise_construct, std::forward_as_tuple(s), std::make_tuple());
			rv.first->second.first.reserve(60 * 24);
			rv.first->second.second.reserve(60 * 24);
        }
    }

	void
		post(const std::string& coin,
			std::chrono::system_clock::time_point time,
			double price)
	{
		const std::lock_guard<std::mutex> guard(mutex_);
		bool update_callback_required = false;
		auto it = entries_.find(coin);
		if (it == entries_.end()) {
			// Unsupported coin - do not record the price
		}
		else if (it->second.first.size() == 0) {
			it->second.first.emplace_back(time, price);
			update_callback_required = true;
		}
		else {
			// We limit the stored prices to one every 5 minutes.
			static const auto one_minute =
				std::chrono::duration_cast<std::chrono::system_clock::duration>(
					std::chrono::seconds(60));
			auto gap = time - it->second.first.back().time_;
			if (gap >= one_minute) {
				it->second.first.emplace_back(time, price);
				update_callback_required = true;
			}
		}
		if (update_handler_ && update_callback_required)
			update_handler_(coin);
		return;
	}

    const std::vector<price_entry>&
        get(const std::string &coin)
    {
		const std::lock_guard<std::mutex> guard(mutex_);
        static const std::vector<price_entry> empty;

        auto pos = entries_.find(coin);
		if (pos != entries_.end()) {
			std::vector<price_entry>& v1 = pos->second.first;
			std::vector<price_entry>& v2 = pos->second.second;
			// Add the missing entries into v2.
			v2.reserve(v1.capacity());
			for (std::size_t i = v2.size(); i < v1.size(); i++) {
				v2.push_back(v1[i]);
			}
			return v2;
		} 
		else
            return empty;
    }
};

#endif

