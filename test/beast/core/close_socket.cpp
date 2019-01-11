//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/close_socket.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/core/get_lowest_layer.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <utility>

namespace boost {
namespace beast {

class close_socket_test : public beast::unit_test::suite
{
public:
    template<class T>
    struct layer
    {
        T t;

        template<class U>
        explicit
        layer(U&& u)
            : t(std::forward<U>(u))
        {
        }

        T& next_layer()
        {
            return t;
        }
    };

    void
    testClose()
    {
        net::io_context ioc;
        {
            net::ip::tcp::socket sock(ioc);
            sock.open(net::ip::tcp::v4());
            BEAST_EXPECT(sock.is_open());
            close_socket(get_lowest_layer(sock));
            BEAST_EXPECT(! sock.is_open());
        }
        {
            layer<net::ip::tcp::socket> layer(ioc);
            layer.next_layer().open(net::ip::tcp::v4());
            BEAST_EXPECT(layer.next_layer().is_open());
            close_socket(get_lowest_layer(layer));
            BEAST_EXPECT(! layer.next_layer().is_open());
        }
    }

    void
    run() override
    {
        testClose();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,close_socket);

} // beast
} // boost
