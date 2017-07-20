//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/dynamic_body.hpp>

#include <boost/beast/core/ostream.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/test/string_istream.hpp>
#include <boost/beast/unit_test/suite.hpp>
#include <boost/lexical_cast.hpp>

namespace boost {
namespace beast {
namespace http {

class dynamic_body_test : public beast::unit_test::suite
{
    boost::asio::io_service ios_;

public:
    void
    run() override
    {
        std::string const s =
            "HTTP/1.1 200 OK\r\n"
            "Server: test\r\n"
            "Content-Length: 3\r\n"
            "\r\n"
            "xyz";
        test::string_istream ss(ios_, s);
        response_parser<dynamic_body> p;
        multi_buffer b;
        read(ss, b, p);
        auto const& m = p.get();
        BOOST_BEAST_EXPECT(boost::lexical_cast<std::string>(
            buffers(m.body.data())) == "xyz");
        BOOST_BEAST_EXPECT(boost::lexical_cast<std::string>(m) == s);
    }
};

BOOST_BEAST_DEFINE_TESTSUITE(dynamic_body,http,beast);

} // http
} // beast
} // boost
