//
// Copyright (c) 2024 Mikhail Khachayants
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/beast/websocket.hpp>
#include <boost/beast/_experimental/test/stream.hpp>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    using namespace boost::beast;

    error_code ec;
    flat_buffer buffer;
    net::io_context ioc;
    test::stream remote{ioc};

    websocket::stream<test::stream> ws{
        ioc, string_view{reinterpret_cast<const char*>(data), size}};

    ws.set_option(websocket::stream_base::decorator(
        [](websocket::response_type& res)
        {
            res.set(http::field::server, "websocket-server-sync");
        }));

    websocket::permessage_deflate pd;
    pd.server_enable = (size % 2) != 0;
    pd.compLevel = static_cast<int>(size % 9);
    ws.set_option(pd);

    ws.next_layer().connect(remote);
    ws.next_layer().close_remote();
    ws.accept(ec);

    if(!ec)
    {
        ws.read(buffer, ec);
        ws.text(ws.got_text());
        ws.write(buffer.data(), ec);
    }

    return 0;
}
