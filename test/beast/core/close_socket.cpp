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
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/core/get_lowest_layer.hpp>
#include <boost/beast/core/string.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
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
            using type = layer<net::ip::tcp::socket>;
            type layer(ioc);
            layer.next_layer().open(net::ip::tcp::v4());
            BEAST_EXPECT(layer.next_layer().is_open());
            BOOST_STATIC_ASSERT(detail::has_next_layer<type>::value);
            BOOST_STATIC_ASSERT((std::is_same<
                typename std::decay<decltype(get_lowest_layer(layer))>::type,
                lowest_layer_type<decltype(layer)>>::value));
            BOOST_STATIC_ASSERT(std::is_same<net::ip::tcp::socket&,
                decltype(get_lowest_layer(layer))>::value);
            BOOST_STATIC_ASSERT(std::is_same<net::ip::tcp::socket,
                lowest_layer_type<decltype(layer)>>::value);
            close_socket(get_lowest_layer(layer));
            BEAST_EXPECT(! layer.next_layer().is_open());
        }
        {
            test::stream ts(ioc);
            close_socket(ts);
        }
    }

    //--------------------------------------------------------------------------

    template <class WriteStream>
    void hello_and_close (WriteStream& stream)
    {
        net::write(stream, net::const_buffer("Hello, world!", 13));
        close_socket(get_lowest_layer(stream));
    }

    class my_socket
    {
        net::ip::tcp::socket sock_;

    public:
        my_socket(net::io_context& ioc)
            : sock_(ioc)
        {
        }

        friend void beast_close_socket(my_socket& s)
        {
            error_code ec;
            s.sock_.close(ec);
            // ignore the error
        }
    };

    void
    testJavadocs()
    {
        BEAST_EXPECT(&close_socket_test::template hello_and_close<net::ip::tcp::socket>);
        {
            net::io_context ioc;
            my_socket s(ioc);
            close_socket(s);
        }
    }

    //--------------------------------------------------------------------------

    void
    run() override
    {
        testClose();
        testJavadocs();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,close_socket);

} // beast
} // boost
