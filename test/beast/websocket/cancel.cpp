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

#include <chrono>

namespace boost {
namespace beast {
namespace websocket {

struct async_all_server_op : boost::asio::coroutine
{
    stream<asio::ip::tcp::socket> & ws;

    async_all_server_op(stream<asio::ip::tcp::socket> & ws) : ws(ws) {}

    template<typename Self>
    void operator()(Self && self, error_code ec = {}, std::size_t = 0)
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
    void operator()(Self && self, error_code ec = {}, std::size_t = 0)
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
                                    // freebsd does this in tests.
                                    || ec == net::error::connection_reset
                                    || ec == net::error::broken_pipe
                                     // winapi WSAECONNRESET, as system_category
                                    || ec == error_code(10054, boost::system::system_category())
                                     // winapi WSAECONNABORTED, as system_category
                                    || ec == error_code(10053, boost::system::system_category())
                                     // winapi ERROR_CONNECTION_ABORTED, as system_category
                                    || ec == error_code(1236, boost::system::system_category())
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
                                      // winapi WSAECONNABORTED, as system_category
                                      || ec == error_code(10053, boost::system::system_category())
                                      // winapi ERROR_CONNECTION_ABORTED, as system_category
                                      || ec == error_code(1236, boost::system::system_category())
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

    // A terminal cancellation emitted after an operation on the
    // next layer completed, but before the websocket operation
    // resumed, has no operation on the next layer to cancel. The
    // websocket operation must observe it instead of starting the
    // next one.
    void
    testCancellationBetweenOps()
    {
        struct fixture
        {
            net::io_context ioc;
            stream<test::stream> ws{ioc};
            test::stream peer{ioc};
            net::cancellation_signal sig;
            bool invoked = false;
            // bytes written to the peer by the handshake
            std::size_t sent = 0;

            struct handler
            {
                fixture& f;
                void operator()(error_code ec, std::size_t) const
                {
                    f.invoked = true;
                    BEAST_EXPECTS(ec == net::error::operation_aborted, ec.message());
                }
            };

            // server role: incoming frames are masked, the payloads
            // are not inspected so the mask key is arbitrary
            explicit fixture(string_view input = {})
            {
                ws.next_layer().connect(peer);
                http::request<http::empty_body> req{http::verb::get, "/", 11};
                req.set(http::field::host, "test");
                req.set(http::field::upgrade, "websocket");
                req.set(http::field::connection, "upgrade");
                req.set(http::field::sec_websocket_key, "dGhlIHNhbXBsZSBub25jZQ==");
                req.set(http::field::sec_websocket_version, "13");
                ws.accept(req);
                sent = peer.nwrite_bytes();
                ws.next_layer().append(input);
                // eof follows the input, so an operation that should
                // have been cancelled fails instead of hanging
                ws.next_layer().close_remote();
            }

            // queued ahead of the first completion from the next
            // layer, so the operation observes it when it resumes
            void post_cancel()
            {
                net::post(ioc, [this]{ sig.emit(net::cancellation_type::terminal); });
            }

            net::cancellation_slot_binder<handler, net::cancellation_slot>
            bind()
            {
                return net::bind_cancellation_slot(sig.slot(), handler{*this});
            }

            // nothing is written to the peer after the cancellation
            void run()
            {
                ioc.run();
                BEAST_EXPECT(invoked);
                BEAST_EXPECT(peer.nwrite_bytes() == sent);
            }
        };

        // read_some: the frame header is split across two reads
        {
            fixture f("\x82");
            f.post_cancel();
            char buf[16];
            f.ws.async_read_some(net::buffer(buf), f.bind());
            f.run();
        }

        // read: the first fragment is not the last one
        {
            fixture f("\x01\x81\x01\x01\x01\x01\x41");
            f.post_cancel();
            flat_buffer b;
            f.ws.async_read(b, f.bind());
            f.run();
        }

        // read_some: the ping is already buffered, the cancellation
        // lands while the operation is dispatched before invoking
        // the control callback, the pong is not written
        {
            fixture f(
                "\x81\x80\x01\x01\x01\x01"
                "\x89\x80\x01\x01\x01\x01");
            f.ws.control_callback([](frame_type, string_view){});
            char buf[16];
            // consumes the text frame, the ping stays buffered
            f.ws.async_read_some(net::buffer(buf),
                [](error_code, std::size_t){});
            f.ioc.run();
            f.ioc.restart();
            f.post_cancel();
            f.ws.async_read_some(net::buffer(buf), f.bind());
            f.run();
        }

        // write: the message is sent as several fragments
        {
            fixture f;
            f.post_cancel();
            f.ws.auto_fragment(true);
            f.ws.write_buffer_bytes(8);
            char const msg[64] = {};
            f.ws.async_write(net::buffer(msg), f.bind());
            f.ioc.run();
            BEAST_EXPECT(f.invoked);
            // only the first fragment was written
            BEAST_EXPECT(f.peer.nwrite_bytes() == f.sent + 2 + 8);
        }

        // write: a ping takes the write lock between two fragments,
        // the cancellation landed before the write is suspended
        {
            fixture f;
            f.post_cancel();
            f.ws.auto_fragment(true);
            f.ws.write_buffer_bytes(8);
            char const msg[16] = {};
            f.ws.async_write(net::buffer(msg), f.bind());
            f.ws.async_ping({}, [](error_code){});
            f.ioc.run();
            BEAST_EXPECT(f.invoked);
            // only the first fragment was written
            BEAST_EXPECT(f.peer.nwrite_bytes() == f.sent + 2 + 8);
        }

        // write: resumed by the ping which held the write lock,
        // and cancelled by the ping's handler before it runs again
        {
            fixture f;
            f.ws.async_ping({},
                [&](error_code)
                {
                    f.sig.emit(net::cancellation_type::terminal);
                });
            char const msg[8] = {};
            f.ws.async_write(net::buffer(msg), f.bind());
            f.ioc.run();
            BEAST_EXPECT(f.invoked);
            // only the ping was written
            BEAST_EXPECT(f.peer.nwrite_bytes() == f.sent + 2);
        }
    }

    // The teardown waits for the peer to close the connection,
    // a cancellation emitted while it waits completes it.
    void
    testTeardownCancellation()
    {
        net::io_context ioc;
        net::ip::tcp::socket s1(ioc), s2(ioc);
        test::connect(s1, s2);
        net::cancellation_signal sig;
        bool invoked = false;
        async_teardown(role_type::server, s1,
            net::bind_cancellation_slot(sig.slot(),
                [&](error_code ec)
                {
                    invoked = true;
                    BEAST_EXPECTS(ec == net::error::operation_aborted, ec.message());
                }));
        net::post(ioc, [&]{ sig.emit(net::cancellation_type::terminal); });
        ioc.run_for(std::chrono::seconds(1));
        BEAST_EXPECT(invoked);
    }

    void
    run() override
    {
        brute_force();
        testCancellationBetweenOps();
        testTeardownCancellation();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,cancel);

} // websocket
} // beast
} // boost
