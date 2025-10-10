//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/buffer_body.hpp>

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/ostream.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/http/serializer.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/_experimental/test/stream.hpp>

namespace boost {
namespace beast {
namespace http {

class buffer_body_test : public beast::unit_test::suite
{
public:
    void
    testIssue1717()
    {
        net::io_context ioc;
        test::stream ts{ioc};
        ostream(ts.buffer()) <<
            "HTTP/1.1 200 OK\r\n"
            "Content-Length:3\r\n"
            "\r\n"
            "1.0";
        error_code ec;
        flat_buffer fb;
        response_parser<buffer_body> p;
        char buf[256];
        p.get().body().data = buf;
        p.get().body().size = sizeof(buf);
        read_header(ts, fb, p, ec);
        auto const bytes_transferred =
            read(ts, fb, p, ec);
        boost::ignore_unused(bytes_transferred);
        BEAST_EXPECTS(! ec, ec.message());

        // VFALCO What should the read algorithms return?
        //BEAST_EXPECT(bytes_transferred == 3);
    }

    void
    testIssue3032()
    {
        net::io_context ioc;
        error_code ec;
        test::stream ts{ioc}, tr{ioc};
        ts.connect(tr);
        response<buffer_body> res;
        response_serializer<buffer_body> sr(res);
        char buf[3] = {'a', 'b', 'c'};

        res.chunked(true);
        res.body().data = buf;
        res.body().size = sizeof(buf);
        write(ts, sr, ec);
        BEAST_EXPECTS(
            ec == error::need_buffer, ec.message());
        BEAST_EXPECTS(
            tr.str() == 
                "HTTP/1.1 200 OK\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                "3\r\nabc\r\n",
            tr.str()
        );

        tr.clear();
        res.body().data = buf;
        res.body().size = 0;
        write(ts, sr, ec);
        BEAST_EXPECTS(
            ec == error::need_buffer, ec.message());
        BEAST_EXPECTS(
            tr.str().empty(), tr.str());

        res.body().more = false;
        write(ts, sr, ec);
        BEAST_EXPECT(sr.is_done());
        BEAST_EXPECTS(! ec, ec.message());
        BEAST_EXPECTS(
            tr.str() == "0\r\n\r\n", tr.str());
    }

    void
    run() override
    {
        testIssue1717();
        testIssue3032();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http,buffer_body);

} // http
} // beast
} // boost
