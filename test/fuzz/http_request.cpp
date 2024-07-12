//
// Copyright (c) 2024 Mikhail Khachayants
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/beast/http.hpp>
#include <boost/beast/_experimental/test/stream.hpp>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    using namespace boost::beast;

    error_code ec;
    flat_buffer buffer;
    net::io_context ioc;
    test::stream stream{ioc, {reinterpret_cast<const char*>(data), size}};
    stream.close_remote();

    http::request_parser<http::dynamic_body> parser;
    http::read(stream, buffer, parser, ec);

    return 0;
}
