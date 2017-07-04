//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <beast/http/file_body_stdc.hpp>

#include <beast/http/empty_body.hpp>
#include <beast/http/read.hpp>
#include <beast/http/string_body.hpp>
#include <beast/http/write.hpp>
#include <beast/test/pipe_stream.hpp>
#include <beast/test/yield_to.hpp>
#include <beast/unit_test/suite.hpp>

namespace beast {
namespace http {

class file_body_stdc_test
    : public beast::unit_test::suite
    , public beast::test::enable_yield_to
{
public:
    // two threads, for some examples using a pipe
    file_body_stdc_test()
        : enable_yield_to(2)
    {
    }

    void
    doFileBody()
    {
        test::pipe c{ios_};

        boost::filesystem::path const path = "temp.txt";
        std::string const body = "Hello, world!\n";
        {
            request<string_body> req;
            req.version = 11;
            req.method(verb::put);
            req.target("/");
            req.body = body;
            req.prepare_payload();
            write(c.client, req);
        }
        {
            flat_buffer b;
            request_parser<empty_body> p0;
            read_header(c.server, b, p0);
            BEAST_EXPECTS(p0.get().method() == verb::put,
                p0.get().method_string());
            {
                error_code ec;
                request_parser<file_body_stdc> p{std::move(p0)};
                p.get().body.open(path, file_mode::write, ec);
                if(ec)
                    BOOST_THROW_EXCEPTION(system_error{ec});
                read(c.server, b, p);
            }
        }
        {
            error_code ec;
            response<file_body_stdc> res;
            res.version = 11;
            res.result(status::ok);
            res.insert(field::server, "test");
            res.body.open(path, file_mode::read, ec);
            if(ec)
                BOOST_THROW_EXCEPTION(system_error{ec});
            res.set(field::content_length, res.body.size());
            write(c.server, res);
        }
        {
            flat_buffer b;
            response<string_body> res;
            read(c.client, b, res);
            BEAST_EXPECTS(res.body == body, body);
        }
        error_code ec;
        boost::filesystem::remove(path, ec);
        BEAST_EXPECTS(! ec, ec.message());
    }

    //--------------------------------------------------------------------------

    void
    run()
    {
        doFileBody();
    }
};

BEAST_DEFINE_TESTSUITE(file_body_stdc,http,beast);

} // http
} // beast
