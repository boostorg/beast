//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <beast/http/string_view_body.hpp>

#include <beast/core/ostream.hpp>
#include <beast/core/static_buffer.hpp>
#include <beast/http/message.hpp>
#include <beast/http/write.hpp>
#include <beast/http/type_traits.hpp>
#include <beast/unit_test/suite.hpp>

namespace beast {
namespace http {

class string_view_body_test
    : public beast::unit_test::suite
{
public:
    void
    run() override
    {
        static_assert(is_body_reader<string_view_body>::value, "");
        static_assert(! is_body_writer<string_view_body>::value, "");
        request<string_view_body> req{"Hello, world!"};
        req.version = 11;
        req.method(verb::post);
        req.target("/");
        req.prepare_payload();
        static_buffer_n<512> b;
        ostream(b) << req;
        string_view const s{
            boost::asio::buffer_cast<char const*>(*b.data().begin()),
            boost::asio::buffer_size(*b.data().begin())};
        BEAST_EXPECT(s ==
            "POST / HTTP/1.1\r\n"
            "Content-Length: 13\r\n"
            "\r\n"
            "Hello, world!");
    }
};

BEAST_DEFINE_TESTSUITE(string_view_body,http,beast);

} // http
} // beast

