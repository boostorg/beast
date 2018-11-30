//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//[snippet_core_1a

#include <boost/beast/core.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <thread>

//]

using namespace boost::beast;

namespace doc_core_snippets {

void fxx()
{
//[snippet_core_1b
//
using namespace boost::beast;

net::io_context ioc;
auto work = net::make_work_guard(ioc);
std::thread t{[&](){ ioc.run(); }};

error_code ec;
net::ip::tcp::socket sock{ioc};

//]
    boost::ignore_unused(ec);

    {
//[snippet_core_2

char const* const host = "www.example.com";
net::ip::tcp::resolver r{ioc};
net::ip::tcp::socket stream{ioc};
auto const results = r.resolve(host, "http");
net::connect(stream, results.begin(), results.end());

// At this point `stream` is a connected to a remote
// host and may be used to perform stream operations.

//]
    }

} // fxx()

//------------------------------------------------------------------------------

//[snippet_core_3

template<class SyncWriteStream>
void write_string(SyncWriteStream& stream, string_view s)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    net::write(stream, net::const_buffer(s.data(), s.size()));
}

//]

} // doc_core_snippets
