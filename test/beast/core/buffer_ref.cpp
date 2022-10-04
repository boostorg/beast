// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/beast/core/buffer_ref.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/core/static_buffer.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/readable_pipe.hpp>
#include <boost/asio/writable_pipe.hpp>
#include <boost/asio/connect_pipe.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/read_until.hpp>
#include "test_buffer.hpp"

namespace boost {
namespace beast {

template struct buffer_ref<flat_buffer>;
template struct buffer_ref<flat_static_buffer<2u>>;
template struct buffer_ref<multi_buffer>;
template struct buffer_ref<static_buffer<2u>>;

class buffer_ref_test : public beast::unit_test::suite
{

    template<typename Buffer>
    void testBuffer()
    {
        net::io_context ioc;
        net::readable_pipe rp{ioc};
        net::writable_pipe wp{ioc};
        net::connect_pipe(rp, wp);

        const char msg[] = "Hello, world!\n";

        net::async_write(wp, net::buffer(msg), asio::detached);

        Buffer buf;

        net::async_read_until(rp, ref(buf), '\n', asio::detached);
        ioc.run();
        // writable, not commited yet!
        std::string cmp;
        cmp.resize(sizeof(msg) -1);
        const auto n = net::buffer_copy(net::buffer(cmp), buf.data());
        BEAST_EXPECT(n >= std::strlen(msg));
        cmp.resize(13);
        BEAST_EXPECT(cmp == "Hello, world!");


        buf = Buffer();
        test_dynamic_buffer_ref(ref(buf));
    }

    void
    run() override
    {
        testBuffer<flat_buffer>();
        testBuffer<flat_static_buffer<1024u>>();
        testBuffer<multi_buffer>();
        testBuffer<static_buffer<1024u>>();

        {
            std::string buf;
            test_dynamic_buffer_ref(asio::dynamic_buffer(buf));
        }

        {
            std::vector<unsigned char> buf;
            test_dynamic_buffer_ref(asio::dynamic_buffer(buf));
        }

    }

};

BEAST_DEFINE_TESTSUITE(beast,core, buffer_ref);

}
}

