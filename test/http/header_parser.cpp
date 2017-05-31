//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <beast/http/header_parser.hpp>

#include <beast/core/flat_buffer.hpp>
#include <beast/http/read.hpp>
#include <beast/unit_test/suite.hpp>
#include <beast/test/string_istream.hpp>
#include <beast/test/yield_to.hpp>

namespace beast {
namespace http {

class header_parser_test
    : public beast::unit_test::suite
    , public test::enable_yield_to
{
public:
    static
    boost::asio::const_buffers_1
    buf(string_view s)
    {
        return {s.data(), s.size()};
    }

    void
    testParse()
    {
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "User-Agent: test\r\n"
                "\r\n"
            };
            flat_buffer db{1024};
            header_parser<true, fields> p;
            read_some(is, db, p);
            BEAST_EXPECT(p.is_header_done());
        }
        {
            test::string_istream is{ios_,
                "POST / HTTP/1.1\r\n"
                "User-Agent: test\r\n"
                "Content-Length: 1\r\n"
                "\r\n"
                "*"
            };
            flat_buffer db{1024};
            header_parser<true, fields> p;
            read_some(is, db, p);
            BEAST_EXPECT(p.is_header_done());
            BEAST_EXPECT(! p.is_done());
        }
    }

    void
    run() override
    {
        testParse();
    }
};

BEAST_DEFINE_TESTSUITE(header_parser,http,beast);

} // http
} // beast

