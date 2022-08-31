//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/core/tcp_stream.hpp>

#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/_experimental/test/tcp.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/redirect_error.hpp>

#include "test.hpp"

namespace boost {
namespace beast {
namespace websocket {

struct async_all_server_op : boost::asio::coroutine
{
    stream<asio::ip::tcp::socket> & ws;

    async_all_server_op(stream<asio::ip::tcp::socket> & ws) : ws(ws) {}

    template<typename Self>
    void operator()(Self && self, error_code ec = {}, std::size_t sz = 0)
    {
        if (ec)
            return self.complete(ec);

        BOOST_ASIO_CORO_REENTER(*this)
        {
            self.reset_cancellation_state([](net::cancellation_type ct){return ct;});
            BOOST_ASIO_CORO_YIELD ws.async_handshake("test", "/", std::move(self));
            BOOST_ASIO_CORO_YIELD ws.async_ping("", std::move(self));
            BOOST_ASIO_CORO_YIELD ws.async_write_some(false, net::buffer("FOO", 3), std::move(self));
            BOOST_ASIO_CORO_YIELD ws.async_write_some(true, net::buffer("BAR", 3), std::move(self));
            BOOST_ASIO_CORO_YIELD ws.async_close("testing", std::move(self));
            self.complete({});
        }
    }
};


template<BOOST_BEAST_ASYNC_TPARAM1 CompletionToken>
BOOST_BEAST_ASYNC_RESULT1(CompletionToken)
async_all_server(
        stream<asio::ip::tcp::socket> & ws,
        CompletionToken && token)
{
    return net::async_compose<CompletionToken, void(error_code)>
            (
                async_all_server_op{ws},
                token, ws
            );
}


struct async_all_client_op : boost::asio::coroutine
{
    stream<asio::ip::tcp::socket> & ws;

    async_all_client_op(stream<asio::ip::tcp::socket> & ws) : ws(ws) {}
    struct impl_t
    {
        impl_t () = default;
        std::string res;
        net::dynamic_string_buffer<char, std::char_traits<char>, std::allocator<char>> buf =
                net::dynamic_buffer(res);
    };

    std::shared_ptr<impl_t> impl{std::make_shared<impl_t>()};

    template<typename Self>
    void operator()(Self && self, error_code ec = {}, std::size_t sz = 0)
    {
        if (ec)
            return self.complete(ec);
        BOOST_ASIO_CORO_REENTER(*this)
        {
            // let everything pass
            self.reset_cancellation_state([](net::cancellation_type ct){return ct;});

            BOOST_ASIO_CORO_YIELD ws.async_accept(std::move(self));
            BOOST_ASIO_CORO_YIELD ws.async_pong("", std::move(self));


            BOOST_ASIO_CORO_YIELD ws.async_read(impl->buf, std::move(self));
            BEAST_EXPECTS(impl->res == "FOOBAR", impl->res);

            BOOST_ASIO_CORO_YIELD ws.async_read(impl->buf, std::move(self));
            BEAST_EXPECTS(ec == websocket::error::closed
                          || ec == net::error::connection_reset
  // hard coded winapi error (WSAECONNRESET), same as connection_reset, but asio delivers it with system_category
                          || (ec.value() == 10054
                              && ec.category() == system::system_category())
                          || ec == net::error::not_connected, ec.message());
            self.complete({});

        }

    }
};

template<BOOST_BEAST_ASYNC_TPARAM1 CompletionToken>
BOOST_BEAST_ASYNC_RESULT1(CompletionToken)
async_all_client(
        stream<asio::ip::tcp::socket> & ws,
        CompletionToken && token)
{
    return net::async_compose<CompletionToken, void(error_code)>
            (
                async_all_client_op{ws},
                token, ws
            );
}

class cancel_test : public websocket_test_suite
{
public:
    std::size_t run_impl(net::cancellation_signal &sl,
                         net::io_context &ctx,
                         std::size_t trigger = -1,
                         net::cancellation_type tp = net::cancellation_type::terminal)
    {
        std::size_t cnt = 0;
        std::size_t res = 0u;
        while ((res = ctx.run_one()) != 0)
            if (trigger == cnt ++)
            {
                sl.emit(tp);
            }

        return cnt;
    }

    std::size_t testAll(std::size_t & cancel_counter,
                        bool cancel_server = false,
                        std::size_t trigger = -1)
    {
        net::cancellation_signal sig1, sig2;

        net::io_context ioc;
        using tcp = net::ip::tcp;

        stream<tcp::socket> ws1(ioc.get_executor());
        stream<tcp::socket> ws2(ioc.get_executor());
        test::connect(ws1.next_layer(), ws2.next_layer());

        async_all_server(ws1,
                net::bind_cancellation_slot(sig1.slot(), [&](system::error_code ec)
                {
                    if (ec)
                    {
                        if (ec == net::error::operation_aborted &&
                            (cancel_server || trigger == static_cast<std::size_t>(-1)))
                            cancel_counter++;

                        BEAST_EXPECTS(ec == net::error::operation_aborted
                                    || ec == net::error::broken_pipe
                                     // winapi WSAECONNRESET, as system_category
                                    || ec == error_code(10054, boost::system::system_category())
                                    || ec == net::error::bad_descriptor
                                    || ec == net::error::eof, ec.message());
                        get_lowest_layer(ws1).close();
                    }
                }));

        async_all_client(
                ws2,
                net::bind_cancellation_slot(sig2.slot(), [&](system::error_code ec)
                {
                    if (ec)
                    {
                        if (ec == net::error::operation_aborted &&
                            (!cancel_server || trigger == static_cast<std::size_t>(-1)))
                            cancel_counter++;
                        BEAST_EXPECTS(ec == net::error::operation_aborted
                                      || ec == error::closed
                                      || ec == net::error::broken_pipe
                                      || ec == net::error::connection_reset
                                      || ec == net::error::not_connected
                                      // winapi WSAECONNRESET, as system_category
                                      || ec == error_code(10054, boost::system::system_category())
                                      || ec == net::error::eof, ec.message());
                        get_lowest_layer(ws1).close();
                    }
                }));

        return run_impl(cancel_server ? sig1 : sig2, ioc, trigger);
    }

    void brute_force()
    {
        std::size_t cancel_counter = 0;
        const auto init = testAll(cancel_counter);
        BEAST_EXPECT(cancel_counter == 0u);
        for (std::size_t cnt = 0; cnt < init; cnt ++)
            testAll(cancel_counter, true, cnt);

        BEAST_EXPECT(cancel_counter > 0u);
        cancel_counter = 0u;
        for (std::size_t cnt = 0; cnt < init; cnt ++)
            testAll(cancel_counter, false, cnt);


        BEAST_EXPECT(cancel_counter > 0u);
    }

    void
    run() override
    {
        brute_force();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,cancel);

} // websocket
} // beast
} // boost
