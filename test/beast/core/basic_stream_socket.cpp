//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/basic_stream_socket.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/core/stream_socket.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>

namespace boost {
namespace beast {

class basic_stream_socket_test
    : public beast::unit_test::suite
{
public:
    struct read_handler
    {
        template<class... Args>
        void operator()(Args&&...)
        {
        }
    };

    template <class Protocol, class Executor, class ReadHandler>
    void async_read_line (
        basic_stream_socket<Protocol, Executor>& stream,
        net::streambuf& buffer, ReadHandler&& handler)
    {
        stream.expires_after(std::chrono::seconds(30));

        net::async_read_until(stream, buffer, "\r\n", std::forward<ReadHandler>(handler));
    }

    void
    testJavadocs()
    {
        BEAST_EXPECT((&basic_stream_socket_test::async_read_line<
            net::ip::tcp, net::io_context::executor_type, read_handler>));
    }

    struct other_t
    {
    };

    void
    testMembers()
    {
        using tcp = net::ip::tcp;
        using ep_t = tcp::endpoint;
        using ioc_t = net::io_context;
        using ex_t = ioc_t::executor_type;
        using stream_t = basic_stream_socket<tcp, ex_t>;

        net::io_context ioc;
        auto ex = ioc.get_executor();

        // construction

        {
            stream_t{ioc};
            stream_t{ex};
            BOOST_STATIC_ASSERT(! std::is_constructible<
                stream_t, other_t>::value);
        }
        {
            stream_t{ioc, tcp::v4()};
            stream_t{ex, tcp::v4()};
            BOOST_STATIC_ASSERT(! std::is_constructible<
                stream_t, other_t, tcp>::value);
        }
        {
            stream_t{ioc, ep_t{}};
            stream_t{ex, ep_t{}};
            BOOST_STATIC_ASSERT(! std::is_constructible<
                stream_t, other_t, ep_t>::value);
        }
        {
            tcp::socket sock(ioc);
            stream_t{ioc, std::move(sock)};
            stream_t{ex, std::move(sock)};
            BOOST_STATIC_ASSERT(! std::is_constructible<
                stream_t, other_t, tcp::socket>::value);
        }

        // move

        {
            stream_t s1(ioc);
            stream_t s2(std::move(s1));
        }
        {
            stream_t s1(ioc);
            stream_t s2(ioc);
            s2 = std::move(s1);
        }

        // converting move

        {
            // We don't have any convertible protocol types
#if 0
            basic_stream_socket<net::ip::udp, ex_t> s1(ioc);
            stream_t s2(std::move(s1));

            stream_t s3 = std::move(s1);
#endif
        }
    }

    void
    run()
    {
        testJavadocs();
        testMembers();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,basic_stream_socket);

} // beast
} // boost
