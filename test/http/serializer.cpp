//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <beast/http/serializer.hpp>

#include <beast/http/string_body.hpp>
#include <beast/unit_test/suite.hpp>

namespace beast {
namespace http {

class serializer_test : public beast::unit_test::suite
{
public:
    struct lambda
    {
        std::size_t size;

        template<class ConstBufferSequence>
        void
        operator()(error_code&,
            ConstBufferSequence const& buffers)
        {
            size = boost::asio::buffer_size(buffers);
        }
    };

    void
    testWriteLimit()
    {
        auto const limit = 30;
        lambda visit;
        error_code ec;
        response<string_body> res;
        res.body.append(1000, '*');
        serializer<false, string_body> sr{res};
        sr.limit(limit);
        for(;;)
        {
            sr.next(ec, visit);
            BEAST_EXPECT(visit.size <= limit);
            sr.consume(visit.size);
            if(sr.is_done())
                break;
        }
    }

    void
    run() override
    {
        testWriteLimit();
    }
};

BEAST_DEFINE_TESTSUITE(serializer,http,beast);

} // http
} // beast
