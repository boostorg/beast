//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/websocket/stream.hpp>

#include "websocket_sync_echo_server.hpp"

#include <boost/beast/core/ostream.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/test/stream.hpp>
#include <boost/beast/test/yield_to.hpp>
#include <boost/beast/unit_test/suite.hpp>
#include <boost/optional.hpp>
#include <memory>
#include <mutex>
#include <condition_variable>

namespace boost {
namespace beast {
namespace websocket {

class stream_test
    : public beast::unit_test::suite
    , public test::enable_yield_to
{
public:
    using self = stream_test;
    using endpoint_type = boost::asio::ip::tcp::endpoint;
    using address_type = boost::asio::ip::address;
    using socket_type = boost::asio::ip::tcp::socket;

    //--------------------------------------------------------------------------

    class asyncEchoServer
        : public std::enable_shared_from_this<asyncEchoServer>
    {
        std::ostream& log_;
        websocket::stream<test::stream> ws_;
        boost::asio::io_service::strand strand_;
        static_buffer<2001> buffer_;

    public:
        asyncEchoServer(
            std::ostream& log,
            test::stream stream)
            : log_(log)
            , ws_(std::move(stream))
            , strand_(ws_.get_io_service())
        {
            permessage_deflate pmd;
            pmd.client_enable = true;
            pmd.server_enable = true;
            ws_.set_option(pmd);
        }

        void
        run()
        {
            ws_.async_accept(strand_.wrap(std::bind(
                &asyncEchoServer::on_accept,
                shared_from_this(),
                std::placeholders::_1)));
        }

        void
        on_accept(error_code ec)
        {
            if(ec)
                return fail(ec);
            do_read();
        }

        void
        do_read()
        {
            ws_.async_read(buffer_,
                strand_.wrap(std::bind(
                    &asyncEchoServer::on_read,
                    shared_from_this(),
                    std::placeholders::_1)));

        }

        void
        on_read(error_code ec)
        {
            if(ec)
                return fail(ec);
            ws_.text(ws_.got_text());
            ws_.async_write(buffer_.data(),
                strand_.wrap(std::bind(
                    &asyncEchoServer::on_write,
                    this->shared_from_this(),
                    std::placeholders::_1)));
        }

        void
        on_write(error_code ec)
        {
            if(ec)
                return fail(ec);
            buffer_.consume(buffer_.size());
            do_read();
        }

        void
        fail(error_code ec)
        {
            if( ec != error::closed &&
                ec != error::failed &&
                ec != boost::asio::error::eof)
                log_ <<
                    "asyncEchoServer: " <<
                    ec.message() <<
                    std::endl;
        }
    };

    void
    echoServer(test::stream& stream)
    {
        try
        {
            websocket::stream<test::stream&> ws{stream};
            permessage_deflate pmd;
            pmd.client_enable = true;
            pmd.server_enable = true;
            ws.set_option(pmd);
            ws.accept();
            for(;;)
            {
                static_buffer<2001> buffer;
                ws.read(buffer);
                ws.text(ws.got_text());
                ws.write(buffer.data());
            }
        }
        catch(system_error const& se)
        {
            if( se.code() != error::closed &&
                se.code() != error::failed &&
                se.code() != boost::asio::error::eof)
                log << "echoServer: " << se.code().message() << std::endl;
        }
        catch(std::exception const& e)
        {
            log << "echoServer: " << e.what() << std::endl;
        }
    }

    void
    launchEchoServer(test::stream stream)
    {
        std::thread{std::bind(
            &stream_test::echoServer,
            this,
            std::move(stream))}.detach();
    }

    void
    launchEchoServerAsync(test::stream stream)
    {
        std::make_shared<asyncEchoServer>(
            log, std::move(stream))->run();
    }

    //--------------------------------------------------------------------------

    using ws_stream_type =
        websocket::stream<test::stream&>;

    template<class Test>
    void
    doTestLoop(Test const& f)
    {
        // This number has to be high for the
        // test that writes the large buffer.
        static std::size_t constexpr limit = 1000;
        std::size_t n;
        for(n = 0; n <= limit; ++n)
        {
            test::fail_counter fc{n};
            test::stream ts{ios_, fc};
            try
            {
                f(ts);

                // Made it through
                ts.close();
                break;
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == test::error::fail_error,
                    se.code().message());
                ts.close();
            }
            catch(std::exception const& e)
            {
                fail(e.what(), __FILE__, __LINE__);
                ts.close();
            }
            continue;
        }
        BEAST_EXPECT(n < limit);
    }
    
    template<class Wrap, class Launch, class Test>
    void
    doTest(
        Wrap const& w,
        permessage_deflate const& pmd,
        Launch const& launch,
        Test const& f)
    {
        doTestLoop(
        [&](test::stream& ts)
        {
            ws_stream_type ws{ts};
            ws.set_option(pmd);
            launch(ts.remote());
            w.handshake(ws, "localhost", "/");
            f(ws);
        });
#if 0
        // Lowering the read_size takes a disproportionate
        // amount of time for the gains in coverage .
        doTestLoop(
        [&](test::stream& ts)
        {
            ws_stream_type ws{ts};
            ts.read_size(45);
            ws.set_option(pmd);
            launch(ts.remote());
            w.handshake(ws, "localhost", "/");
            f(ws);
        });
#endif
    }

    template<class Wrap>
    void
    doCloseTest(
        Wrap const& w,
        ws_stream_type& ws,
        close_code code)
    {
        try
        {
            multi_buffer b;
            w.read(ws, b);
            fail("", __FILE__, __LINE__);
        }
        catch(system_error const& se)
        {
            if(se.code() != error::closed)
                throw;
            BEAST_EXPECT(
                ws.reason().code == code);
        }
    }

    template<class Wrap>
    void
    doFailTest(
        Wrap const& w,
        ws_stream_type& ws,
        error_code ev)
    {
        try
        {
            multi_buffer b;
            w.read(ws, b);
            fail("", __FILE__, __LINE__);
        }
        catch(system_error const& se)
        {
            if(se.code() != ev)
                throw;
        }
    }

    //--------------------------------------------------------------------------

    template<class ConstBufferSequence>
    static
    std::string
    to_string(ConstBufferSequence const& bs)
    {
        using boost::asio::buffer_cast;
        using boost::asio::buffer_size;
        std::string s;
        s.reserve(buffer_size(bs));
        for(boost::asio::const_buffer b : bs)
            s.append(buffer_cast<char const*>(b),
                buffer_size(b));
        return s;
    }

    template<std::size_t N>
    class cbuf_helper
    {
        std::array<std::uint8_t, N> v_;
        boost::asio::const_buffer cb_;

    public:
        using value_type = decltype(cb_);
        using const_iterator = value_type const*;

        template<class... Vn>
        explicit
        cbuf_helper(Vn... vn)
            : v_({{ static_cast<std::uint8_t>(vn)... }})
            , cb_(v_.data(), v_.size())
        {
        }

        const_iterator
        begin() const
        {
            return &cb_;
        }

        const_iterator
        end() const
        {
            return begin()+1;
        }
    };

    template<class... Vn>
    cbuf_helper<sizeof...(Vn)>
    cbuf(Vn... vn)
    {
        return cbuf_helper<sizeof...(Vn)>(vn...);
    }

    template<std::size_t N>
    static
    boost::asio::const_buffers_1
    sbuf(const char (&s)[N])
    {
        return boost::asio::const_buffers_1(&s[0], N-1);
    }

    template<
        class DynamicBuffer,
        class ConstBufferSequence>
    static
    void
    put(
        DynamicBuffer& buffer,
        ConstBufferSequence const& buffers)
    {
        using boost::asio::buffer_copy;
        using boost::asio::buffer_size;
        buffer.commit(buffer_copy(
            buffer.prepare(buffer_size(buffers)),
            buffers));
    }

    template<class Pred>
    static
    bool
    run_until(boost::asio::io_service& ios,
        std::size_t limit, Pred&& pred)
    {
        for(std::size_t i = 0; i < limit; ++i)
        {
            if(pred())
                return true;
            ios.run_one();
        }
        return false;
    }

    //--------------------------------------------------------------------------

    struct SyncClient
    {
        template<class NextLayer>
        void
        accept(stream<NextLayer>& ws) const
        {
            ws.accept();
        }

        template<class NextLayer, class Buffers>
        typename std::enable_if<
            ! http::detail::is_header<Buffers>::value>::type
        accept(stream<NextLayer>& ws,
            Buffers const& buffers) const
        {
            ws.accept(buffers);
        }

        template<class NextLayer>
        void
        accept(stream<NextLayer>& ws,
            http::request<http::empty_body> const& req) const
        {
            ws.accept(req);
        }

        template<class NextLayer, class Decorator>
        void
        accept_ex(stream<NextLayer>& ws,
            Decorator const& d) const
        {
            ws.accept_ex(d);
        }

        template<class NextLayer,
            class Buffers, class Decorator>
        typename std::enable_if<
            ! http::detail::is_header<Buffers>::value>::type
        accept_ex(stream<NextLayer>& ws,
            Buffers const& buffers,
                Decorator const& d) const
        {
            ws.accept_ex(buffers, d);
        }

        template<class NextLayer, class Decorator>
        void
        accept_ex(stream<NextLayer>& ws,
            http::request<http::empty_body> const& req,
                Decorator const& d) const
        {
            ws.accept_ex(req, d);
        }

        template<class NextLayer,
            class Buffers, class Decorator>
        void
        accept_ex(stream<NextLayer>& ws,
            http::request<http::empty_body> const& req,
                Buffers const& buffers,
                    Decorator const& d) const
        {
            ws.accept_ex(req, buffers, d);
        }

        template<class NextLayer>
        void
        handshake(stream<NextLayer>& ws,
            string_view uri,
                string_view path) const
        {
            ws.handshake(uri, path);
        }

        template<class NextLayer>
        void
        handshake(stream<NextLayer>& ws,
            response_type& res,
                string_view uri,
                    string_view path) const
        {
            ws.handshake(res, uri, path);
        }

        template<class NextLayer, class Decorator>
        void
        handshake_ex(stream<NextLayer>& ws,
            string_view uri,
                string_view path,
                    Decorator const& d) const
        {
            ws.handshake_ex(uri, path, d);
        }

        template<class NextLayer, class Decorator>
        void
        handshake_ex(stream<NextLayer>& ws,
            response_type& res,
                string_view uri,
                    string_view path,
                        Decorator const& d) const
        {
            ws.handshake_ex(res, uri, path, d);
        }

        template<class NextLayer>
        void
        ping(stream<NextLayer>& ws,
            ping_data const& payload) const
        {
            ws.ping(payload);
        }

        template<class NextLayer>
        void
        pong(stream<NextLayer>& ws,
            ping_data const& payload) const
        {
            ws.pong(payload);
        }

        template<class NextLayer>
        void
        close(stream<NextLayer>& ws,
            close_reason const& cr) const
        {
            ws.close(cr);
        }

        template<
            class NextLayer, class DynamicBuffer>
        std::size_t
        read(stream<NextLayer>& ws,
            DynamicBuffer& buffer) const
        {
            return ws.read(buffer);
        }

        template<
            class NextLayer, class DynamicBuffer>
        std::size_t
        read_some(stream<NextLayer>& ws,
            std::size_t limit,
            DynamicBuffer& buffer) const
        {
            return ws.read_some(buffer, limit);
        }

        template<
            class NextLayer, class MutableBufferSequence>
        std::size_t
        read_some(stream<NextLayer>& ws,
            MutableBufferSequence const& buffers) const
        {
            return ws.read_some(buffers);
        }

        template<
            class NextLayer, class ConstBufferSequence>
        void
        write(stream<NextLayer>& ws,
            ConstBufferSequence const& buffers) const
        {
            ws.write(buffers);
        }

        template<
            class NextLayer, class ConstBufferSequence>
        void
        write_some(stream<NextLayer>& ws, bool fin,
            ConstBufferSequence const& buffers) const
        {
            ws.write_some(fin, buffers);
        }

        template<
            class NextLayer, class ConstBufferSequence>
        void
        write_raw(stream<NextLayer>& ws,
            ConstBufferSequence const& buffers) const
        {
            boost::asio::write(
                ws.next_layer(), buffers);
        }
    };

    //--------------------------------------------------------------------------

    class AsyncClient
    {
        yield_context& yield_;

    public:
        explicit
        AsyncClient(yield_context& yield)
            : yield_(yield)
        {
        }

        template<class NextLayer>
        void
        accept(stream<NextLayer>& ws) const
        {
            error_code ec;
            ws.async_accept(yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer, class Buffers>
        typename std::enable_if<
            ! http::detail::is_header<Buffers>::value>::type
        accept(stream<NextLayer>& ws,
            Buffers const& buffers) const
        {
            error_code ec;
            ws.async_accept(buffers, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer>
        void
        accept(stream<NextLayer>& ws,
            http::request<http::empty_body> const& req) const
        {
            error_code ec;
            ws.async_accept(req, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer,
            class Decorator>
        void
        accept_ex(stream<NextLayer>& ws,
            Decorator const& d) const
        {
            error_code ec;
            ws.async_accept_ex(d, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer,
            class Buffers, class Decorator>
        typename std::enable_if<
            ! http::detail::is_header<Buffers>::value>::type
        accept_ex(stream<NextLayer>& ws,
            Buffers const& buffers,
                Decorator const& d) const
        {
            error_code ec;
            ws.async_accept_ex(buffers, d, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer, class Decorator>
        void
        accept_ex(stream<NextLayer>& ws,
            http::request<http::empty_body> const& req,
                Decorator const& d) const
        {
            error_code ec;
            ws.async_accept_ex(req, d, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer,
            class Buffers, class Decorator>
        void
        accept_ex(stream<NextLayer>& ws,
            http::request<http::empty_body> const& req,
                Buffers const& buffers,
                    Decorator const& d) const
        {
            error_code ec;
            ws.async_accept_ex(
                req, buffers, d, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer>
        void
        handshake(stream<NextLayer>& ws,
            string_view uri,
                string_view path) const
        {
            error_code ec;
            ws.async_handshake(
                uri, path, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer>
        void
        handshake(stream<NextLayer>& ws,
            response_type& res,
                string_view uri,
                    string_view path) const
        {
            error_code ec;
            ws.async_handshake(
                res, uri, path, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer, class Decorator>
        void
        handshake_ex(stream<NextLayer>& ws,
            string_view uri,
                string_view path,
                    Decorator const &d) const
        {
            error_code ec;
            ws.async_handshake_ex(
                uri, path, d, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer, class Decorator>
        void
        handshake_ex(stream<NextLayer>& ws,
            response_type& res,
                string_view uri,
                    string_view path,
                        Decorator const &d) const
        {
            error_code ec;
            ws.async_handshake_ex(
                res, uri, path, d, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer>
        void
        ping(stream<NextLayer>& ws,
            ping_data const& payload) const
        {
            error_code ec;
            ws.async_ping(payload, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer>
        void
        pong(stream<NextLayer>& ws,
            ping_data const& payload) const
        {
            error_code ec;
            ws.async_pong(payload, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<class NextLayer>
        void
        close(stream<NextLayer>& ws,
            close_reason const& cr) const
        {
            error_code ec;
            ws.async_close(cr, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<
            class NextLayer, class DynamicBuffer>
        std::size_t
        read(stream<NextLayer>& ws,
            DynamicBuffer& buffer) const
        {
            error_code ec;
            auto const bytes_written =
                ws.async_read(buffer, yield_[ec]);
            if(ec)
                throw system_error{ec};
            return bytes_written;
        }

        template<
            class NextLayer, class DynamicBuffer>
        std::size_t
        read_some(stream<NextLayer>& ws,
            std::size_t limit,
            DynamicBuffer& buffer) const
        {
            error_code ec;
            auto const bytes_written =
                ws.async_read_some(buffer, limit, yield_[ec]);
            if(ec)
                throw system_error{ec};
            return bytes_written;
        }

        template<
            class NextLayer, class MutableBufferSequence>
        std::size_t
        read_some(stream<NextLayer>& ws,
            MutableBufferSequence const& buffers) const
        {
            error_code ec;
            auto const bytes_written =
                ws.async_read_some(buffers, yield_[ec]);
            if(ec)
                throw system_error{ec};
            return bytes_written;
        }

        template<
            class NextLayer, class ConstBufferSequence>
        void
        write(stream<NextLayer>& ws,
            ConstBufferSequence const& buffers) const
        {
            error_code ec;
            ws.async_write(buffers, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<
            class NextLayer, class ConstBufferSequence>
        void
        write_some(stream<NextLayer>& ws, bool fin,
            ConstBufferSequence const& buffers) const
        {
            error_code ec;
            ws.async_write_some(fin, buffers, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }

        template<
            class NextLayer, class ConstBufferSequence>
        void
        write_raw(stream<NextLayer>& ws,
            ConstBufferSequence const& buffers) const
        {
            error_code ec;
            boost::asio::async_write(
                ws.next_layer(), buffers, yield_[ec]);
            if(ec)
                throw system_error{ec};
        }
    };

    //--------------------------------------------------------------------------

    void
    testOptions()
    {
        stream<socket_type> ws(ios_);
        ws.auto_fragment(true);
        ws.write_buffer_size(2048);
        ws.binary(false);
        ws.read_message_max(1 * 1024 * 1024);
        try
        {
            ws.write_buffer_size(7);
            fail();
        }
        catch(std::exception const&)
        {
            pass();
        }
    }

    //--------------------------------------------------------------------------
    //
    // Accept
    //
    //--------------------------------------------------------------------------

    template<class Client>
    void
    doTestAccept(Client const& c)
    {
        class res_decorator
        {
            bool& b_;

        public:
            res_decorator(res_decorator const&) = default;

            explicit
            res_decorator(bool& b)
                : b_(b)
            {
            }

            void
            operator()(response_type&) const
            {
                b_ = true;
            }
        };

        auto const big = []
        {
            std::string s;
            s += "X1: " + std::string(2000, '*') + "\r\n";
            return s;
        }();

        // request in stream
        doTestLoop([&](test::stream& ts)
        {
            stream<test::stream&> ws{ts};
            ts.str(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n");
            ts.read_size(20);
            c.accept(ws);
            // VFALCO validate contents of ws.next_layer().str?
        });

        // request in stream, oversized
        {
            stream<test::stream> ws{ios_,
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                + big +
                "\r\n"};
            error_code ec = test::error::fail_error;
            try
            {
                c.accept(ws);
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                // VFALCO Its the http error category...
                BEAST_EXPECTS(
                    se.code() == http::error::buffer_overflow,
                    se.code().message());
            }
        }

        // request in stream, decorator
        doTestLoop([&](test::stream& ts)
        {
            stream<test::stream&> ws{ts};
            ts.str(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n");
            ts.read_size(20);
            bool called = false;
            c.accept_ex(ws, res_decorator{called});
            BEAST_EXPECT(called);
        });

        // request in stream, decorator, oversized
        {
            stream<test::stream> ws{ios_,
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                + big +
                "\r\n"};
            try
            {
                bool called = false;
                c.accept_ex(ws, res_decorator{called});
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                // VFALCO Its the http error category...
                BEAST_EXPECTS(
                    se.code() == http::error::buffer_overflow,
                    se.code().message());
            }
        }

        // request in buffers
        doTestLoop([&](test::stream& ts)
        {
            stream<test::stream&> ws{ts};
            c.accept(ws, sbuf(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n"
            ));
        });

        // request in buffers, oversize
        {
            stream<test::stream> ws{ios_};
            try
            {
                c.accept(ws, boost::asio::buffer(
                    "GET / HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: upgrade\r\n"
                    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                    "Sec-WebSocket-Version: 13\r\n"
                    + big +
                    "\r\n"
                ));
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == error::buffer_overflow,
                    se.code().message());
            }
        }

        // request in buffers, decorator
        doTestLoop([&](test::stream& ts)
        {
            stream<test::stream&> ws{ts};
            bool called = false;
            c.accept_ex(ws, sbuf(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n"),
                res_decorator{called});
            BEAST_EXPECT(called);
        });

        // request in buffers, decorator, oversized
        {
            stream<test::stream> ws{ios_};
            try
            {
                bool called = false;
                c.accept_ex(ws, boost::asio::buffer(
                    "GET / HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: upgrade\r\n"
                    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                    "Sec-WebSocket-Version: 13\r\n"
                    + big +
                    "\r\n"),
                    res_decorator{called});
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == error::buffer_overflow,
                    se.code().message());
            }
        }

        // request in buffers and stream
        doTestLoop([&](test::stream& ts)
        {
            stream<test::stream&> ws{ts};
            ts.str(
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n");
            ts.read_size(16);
            c.accept(ws, sbuf(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
            ));
            // VFALCO validate contents of ws.next_layer().str?
        });

        // request in buffers and stream, oversized
        {
            stream<test::stream> ws{ios_,
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                + big +
                "\r\n"};
            try
            {
                c.accept(ws, sbuf(
                    "GET / HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Upgrade: websocket\r\n"
                ));
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == http::error::buffer_overflow,
                    se.code().message());
            }
        }

        // request in buffers and stream, decorator
        doTestLoop([&](test::stream& ts)
        {
            stream<test::stream&> ws{ts};
            ts.str(
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n");
            ts.read_size(16);
            bool called = false;
            c.accept_ex(ws, sbuf(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"),
                res_decorator{called});
            BEAST_EXPECT(called);
        });

        // request in buffers and stream, decorator, oversize
        {
            stream<test::stream> ws{ios_,
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                + big +
                "\r\n"};
            try
            {
                bool called = false;
                c.accept_ex(ws, sbuf(
                    "GET / HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Upgrade: websocket\r\n"),
                    res_decorator{called});
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == http::error::buffer_overflow,
                    se.code().message());
            }
        }

        // request in message
        doTestLoop([&](test::stream& ts)
        {
            stream<test::stream&> ws{ts};
            request_type req;
            req.method(http::verb::get);
            req.target("/");
            req.version = 11;
            req.insert(http::field::host, "localhost");
            req.insert(http::field::upgrade, "websocket");
            req.insert(http::field::connection, "upgrade");
            req.insert(http::field::sec_websocket_key, "dGhlIHNhbXBsZSBub25jZQ==");
            req.insert(http::field::sec_websocket_version, "13");
            c.accept(ws, req);
        });

        // request in message, decorator
        doTestLoop([&](test::stream& ts)
        {
            stream<test::stream&> ws{ts};
            request_type req;
            req.method(http::verb::get);
            req.target("/");
            req.version = 11;
            req.insert(http::field::host, "localhost");
            req.insert(http::field::upgrade, "websocket");
            req.insert(http::field::connection, "upgrade");
            req.insert(http::field::sec_websocket_key, "dGhlIHNhbXBsZSBub25jZQ==");
            req.insert(http::field::sec_websocket_version, "13");
            bool called = false;
            c.accept_ex(ws, req,
                res_decorator{called});
            BEAST_EXPECT(called);
        });

        // request in message, close frame in stream
        doTestLoop([&](test::stream& ts)
        {
            stream<test::stream&> ws{ts};
            request_type req;
            req.method(http::verb::get);
            req.target("/");
            req.version = 11;
            req.insert(http::field::host, "localhost");
            req.insert(http::field::upgrade, "websocket");
            req.insert(http::field::connection, "upgrade");
            req.insert(http::field::sec_websocket_key, "dGhlIHNhbXBsZSBub25jZQ==");
            req.insert(http::field::sec_websocket_version, "13");
            ts.str("\x88\x82\xff\xff\xff\xff\xfc\x17");
            c.accept(ws, req);
            try
            {
                static_buffer<1> b;
                c.read(ws, b);
                fail("success", __FILE__, __LINE__);
            }
            catch(system_error const& e)
            {
                if(e.code() != websocket::error::closed)
                    throw;
            }
        });

        // failed handshake (missing Sec-WebSocket-Key)
        doTestLoop([&](test::stream& ts)
        {
            stream<test::stream&> ws{ts};
            ts.str(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n");
            ts.read_size(20);
            try
            {
                c.accept(ws);
                fail("success", __FILE__, __LINE__);
            }
            catch(system_error const& e)
            {
                if( e.code() !=
                        websocket::error::handshake_failed &&
                    e.code() !=
                        boost::asio::error::eof)
                    throw;
            }
        });

        // Closed by client
        {
            stream<test::stream> ws{ios_};
            ws.next_layer().remote().close();
            try
            {
                c.accept(ws);
                fail("success", __FILE__, __LINE__);
            }
            catch(system_error const& e)
            {
                if(! BEAST_EXPECTS(
                    e.code() == error::closed,
                    e.code().message()))
                    throw;
            }
        }
    }

    void
    testAccept()
    {
        doTestAccept(SyncClient{});

        yield_to([&](yield_context yield)
        {
            doTestAccept(AsyncClient{yield});
        });

        //
        // Bad requests
        //

        auto const check =
        [&](error_code const& ev, std::string const& s)
        {
            for(int i = 0; i < 3; ++i)
            {
                std::size_t n;
                switch(i)
                {
                default:
                case 0:
                    n = 1;
                    break;
                case 1:
                    n = s.size() / 2;
                    break;
                case 2:
                    n = s.size() - 1;
                    break;
                }
                stream<test::stream> ws{ios_};
                ws.next_layer().str(
                    s.substr(n, s.size() - n));
                try
                {
                    ws.accept(
                        boost::asio::buffer(s.data(), n));
                    BEAST_EXPECTS(! ev, ev.message());
                }
                catch(system_error const& se)
                {
                    BEAST_EXPECTS(se.code() == ev, se.what());
                }
            }
        };

        // wrong version
        check(http::error::end_of_stream,
            "GET / HTTP/1.0\r\n"
            "Host: localhost:80\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: keep-alive,upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // wrong method
        check(error::handshake_failed,
            "POST / HTTP/1.1\r\n"
            "Host: localhost:80\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: keep-alive,upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // missing Host
        check(error::handshake_failed,
            "GET / HTTP/1.1\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: keep-alive,upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // missing Sec-WebSocket-Key
        check(error::handshake_failed,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:80\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: keep-alive,upgrade\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // missing Sec-WebSocket-Version
        check(error::handshake_failed,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:80\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: keep-alive,upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "\r\n"
        );
        // wrong Sec-WebSocket-Version
        check(error::handshake_failed,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:80\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: keep-alive,upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 1\r\n"
            "\r\n"
        );
        // missing upgrade token
        check(error::handshake_failed,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:80\r\n"
            "Upgrade: HTTP/2\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // missing connection token
        check(error::handshake_failed,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:80\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: keep-alive\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // valid request
        check({},
            "GET / HTTP/1.1\r\n"
            "Host: localhost:80\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
    }

    //--------------------------------------------------------------------------
    //
    // Close
    //
    //--------------------------------------------------------------------------

    template<class Wrap>
    void
    doTestClose(Wrap const& w)
    {
        permessage_deflate pmd;
        pmd.client_enable = false;
        pmd.server_enable = false;

        auto const launch =
        [&](test::stream stream)
        {
            launchEchoServerAsync(std::move(stream));
        };

        // normal close
        doTest(w, pmd, launch,
        [&](ws_stream_type& ws)
        {
            w.close(ws, {});
        });

        // double close
        {
            stream<test::stream> ws{ios_};
            launch(ws.next_layer().remote());
            w.handshake(ws, "localhost", "/");
            w.close(ws, {});
            try
            {
                w.close(ws, {});
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == boost::asio::error::operation_aborted,
                    se.code().message());
            }
        }

        // drain a message after close
        doTest(w, pmd, launch,
        [&](ws_stream_type& ws)
        {
            ws.next_layer().str("\x81\x01\x2a");
            w.close(ws, {});
        });

        // drain a big message after close
        {
            std::string s;
            s = "\x81\x7e\x10\x01";
            s.append(4097, '*');
            doTest(w, pmd, launch,
            [&](ws_stream_type& ws)
            {
                ws.next_layer().str(s);
                w.close(ws, {});
            });
        }

        // drain a ping after close
        doTest(w, pmd, launch,
        [&](ws_stream_type& ws)
        {
            ws.next_layer().str("\x89\x01*");
            w.close(ws, {});
        });

        // drain invalid message frame after close
        {
            stream<test::stream> ws{ios_};
            launch(ws.next_layer().remote());
            w.handshake(ws, "localhost", "/");
            ws.next_layer().str("\x81\x81\xff\xff\xff\xff*");
            try
            {
                w.close(ws, {});
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == error::failed,
                    se.code().message());
            }
        }

        // drain invalid close frame after close
        {
            stream<test::stream> ws{ios_};
            launch(ws.next_layer().remote());
            w.handshake(ws, "localhost", "/");
            ws.next_layer().str("\x88\x01*");
            try
            {
                w.close(ws, {});
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == error::failed,
                    se.code().message());
            }
        }

        // close with incomplete read message
        doTest(w, pmd, launch,
        [&](ws_stream_type& ws)
        {
            ws.next_layer().str("\x81\x02**");
            static_buffer<1> b;
            w.read_some(ws, 1, b);
            w.close(ws, {});
        });
    }

    void
    testClose()
    {
        doTestClose(SyncClient{});

        yield_to([&](yield_context yield)
        {
            doTestClose(AsyncClient{yield});
        });

        auto const launch =
        [&](test::stream stream)
        {
            launchEchoServer(std::move(stream));
            //launchEchoServerAsync(std::move(stream));
        };

        // suspend on write
        {
            error_code ec;
            boost::asio::io_service ios;
            stream<test::stream> ws{ios, ios_};
            launch(ws.next_layer().remote());
            ws.handshake("localhost", "/", ec);
            BEAST_EXPECTS(! ec, ec.message());
            std::size_t count = 0;
            ws.async_ping("",
                [&](error_code ec)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            BEAST_EXPECT(ws.wr_block_);
            ws.async_close({},
                [&](error_code ec)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            ios.run();
            BEAST_EXPECT(count == 2);
        }

        // suspend on read
        {
            error_code ec;
            boost::asio::io_service ios;
            stream<test::stream> ws{ios, ios_};
            launch(ws.next_layer().remote());
            ws.handshake("localhost", "/", ec);
            BEAST_EXPECTS(! ec, ec.message());
            flat_buffer b;
            std::size_t count = 0;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    ++count;
                    BEAST_EXPECTS(
                        ec == error::closed, ec.message());
                });
            BEAST_EXPECT(ws.rd_block_);
            ws.async_close({},
                [&](error_code ec)
                {
                    ++count;
                    BEAST_EXPECTS(
                        ec == boost::asio::error::operation_aborted,
                        ec.message());
                });
            BEAST_EXPECT(ws.wr_close_);
            ios.run();
            BEAST_EXPECT(count == 2);
        }
    }

    //--------------------------------------------------------------------------

    template<class Client, class Launch>
    void
    doTestHandshake(Client const& c, Launch const& launch)
    {
        class req_decorator
        {
            bool& b_;

        public:
            req_decorator(req_decorator const&) = default;

            explicit
            req_decorator(bool& b)
                : b_(b)
            {
            }

            void
            operator()(request_type&) const
            {
                b_ = true;
            }
        };

        // handshake
        doTestLoop([&](test::stream& ts)
        {
            ws_stream_type ws{ts};
            launch(ts.remote());
            c.handshake(ws, "localhost", "/");
        });

        // handshake, response
        doTestLoop([&](test::stream& ts)
        {
            ws_stream_type ws{ts};
            launch(ts.remote());
            response_type res;
            c.handshake(ws, res, "localhost", "/");
            // VFALCO validate res?
        });

        // handshake, decorator
        doTestLoop([&](test::stream& ts)
        {
            ws_stream_type ws{ts};
            launch(ts.remote());
            bool called = false;
            c.handshake_ex(ws, "localhost", "/",
                req_decorator{called});
            BEAST_EXPECT(called);
        });

        // handshake, response, decorator
        doTestLoop([&](test::stream& ts)
        {
            ws_stream_type ws{ts};
            launch(ts.remote());
            bool called = false;
            response_type res;
            c.handshake_ex(ws, res, "localhost", "/",
                req_decorator{called});
            // VFALCO validate res?
            BEAST_EXPECT(called);
        });
    }

    void
    testHandshake()
    {
        doTestHandshake(SyncClient{},
        [&](test::stream stream)
        {
            launchEchoServer(std::move(stream));
        });

        yield_to([&](yield_context yield)
        {
            doTestHandshake(AsyncClient{yield},
            [&](test::stream stream)
            {
                launchEchoServerAsync(std::move(stream));
            });
        });

        auto const check =
        [&](std::string const& s)
        {
            stream<test::stream> ws{ios_};
            ws.next_layer().str(s);
            ws.next_layer().remote().close();
            try
            {
                ws.handshake("localhost:80", "/");
                fail();
            }
            catch(system_error const& se)
            {
                BEAST_EXPECT(se.code() == error::handshake_failed);
            }
        };
        // wrong HTTP version
        check(
            "HTTP/1.0 101 Switching Protocols\r\n"
            "Server: beast\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // wrong status
        check(
            "HTTP/1.1 200 OK\r\n"
            "Server: beast\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // missing upgrade token
        check(
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Server: beast\r\n"
            "Upgrade: HTTP/2\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // missing connection token
        check(
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Server: beast\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: keep-alive\r\n"
            "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // missing accept key
        check(
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Server: beast\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // wrong accept key
        check(
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Server: beast\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Accept: *\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
    }

    //--------------------------------------------------------------------------
    //
    // Ping
    //
    //--------------------------------------------------------------------------

    template<class Wrap>
    void
    doTestPing(Wrap const& w)
    {
        auto const launch =
        [&](test::stream stream)
        {
            launchEchoServer(std::move(stream));
            //launchEchoServerAsync(std::move(stream));
        };

        permessage_deflate pmd;
        pmd.client_enable = false;
        pmd.server_enable = false;

        // ping
        doTest(w, pmd, launch, [&](ws_stream_type& ws)
        {
            w.ping(ws, {});
        });

        // pong
        doTest(w, pmd, launch, [&](ws_stream_type& ws)
        {
            w.pong(ws, {});
        });
    }

    void
    testPing()
    {
        doTestPing(SyncClient{});

        yield_to([&](yield_context yield)
        {
            doTestPing(AsyncClient{yield});
        });

        auto const launch =
        [&](test::stream stream)
        {
            launchEchoServer(std::move(stream));
            //launchEchoServerAsync(std::move(stream));
        };

        // ping, already closed
        {
            stream<test::stream> ws{ios_};
            error_code ec;
            ws.ping({}, ec);
            BEAST_EXPECTS(
                ec == boost::asio::error::operation_aborted,
                ec.message());
        }

        // async_ping, already closed
        {
            boost::asio::io_service ios;
            stream<test::stream> ws{ios};
            ws.async_ping({},
                [&](error_code ec)
                {
                    BEAST_EXPECTS(
                        ec == boost::asio::error::operation_aborted,
                        ec.message());
                });
            ios.run();
        }

        // pong, already closed
        {
            stream<test::stream> ws{ios_};
            error_code ec;
            ws.pong({}, ec);
            BEAST_EXPECTS(
                ec == boost::asio::error::operation_aborted,
                ec.message());
        }

        // async_pong, already closed
        {
            boost::asio::io_service ios;
            stream<test::stream> ws{ios};
            ws.async_pong({},
                [&](error_code ec)
                {
                    BEAST_EXPECTS(
                        ec == boost::asio::error::operation_aborted,
                        ec.message());
                });
            ios.run();
        }

        // suspend on write
        {
            error_code ec;
            boost::asio::io_service ios;
            stream<test::stream> ws{ios, ios_};
            launch(ws.next_layer().remote());
            ws.handshake("localhost", "/", ec);
            BEAST_EXPECTS(! ec, ec.message());
            std::size_t count = 0;
            ws.async_write(sbuf("*"),
                [&](error_code ec)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                });
            BEAST_EXPECT(ws.wr_block_);
            ws.async_ping("",
                [&](error_code ec)
                {
                    ++count;
                    BEAST_EXPECTS(
                        ec == boost::asio::error::operation_aborted,
                        ec.message());
                });
            ws.async_close({}, [&](error_code){});
            ios.run();
            BEAST_EXPECT(count == 2);
        }
    }

    //--------------------------------------------------------------------------
    //
    // Read
    //
    //--------------------------------------------------------------------------

    void
    testRead()
    {
        // Read close frames
        {
            auto const check =
            [&](error_code ev, string_view s)
            {
                test::stream ts{ios_};
                stream<test::stream&> ws{ts};
                launchEchoServerAsync(ts.remote());
                ws.handshake("localhost", "/");
                ts.str(s);
                static_buffer<1> b;
                error_code ec;
                ws.read(b, ec);
                BEAST_EXPECTS(ec == ev, ec.message());
            };

            // payload length 1
            check(error::failed,
                "\x88\x01\x01");

            // invalid close code 1005
            check(error::failed,
                "\x88\x02\x03\xed");

            // invalid utf8
            check(error::failed,
                "\x88\x06\xfc\x15\x0f\xd7\x73\x43");

            // good utf8
            check(error::closed,
                "\x88\x06\xfc\x15utf8");
        }
    }

    //--------------------------------------------------------------------------

    void
    testMask(endpoint_type const& ep,
        yield_context do_yield)
    {
        {
            std::vector<char> v;
            for(char n = 0; n < 20; ++n)
            {
                error_code ec = test::error::fail_error;
                socket_type sock(ios_);
                sock.connect(ep, ec);
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    break;
                stream<socket_type&> ws(sock);
                ws.handshake("localhost", "/", ec);
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    break;
                ws.write(boost::asio::buffer(v), ec);
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    break;
                multi_buffer b;
                ws.read(b, ec);
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    break;
                BEAST_EXPECT(to_string(b.data()) ==
                    std::string(v.data(), v.size()));
                v.push_back(n+1);
            }
        }
        {
            std::vector<char> v;
            for(char n = 0; n < 20; ++n)
            {
                error_code ec = test::error::fail_error;
                socket_type sock(ios_);
                sock.connect(ep, ec);
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    break;
                stream<socket_type&> ws(sock);
                ws.handshake("localhost", "/", ec);
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    break;
                ws.async_write(boost::asio::buffer(v), do_yield[ec]);
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    break;
                multi_buffer b;
                ws.async_read(b, do_yield[ec]);
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    break;
                BEAST_EXPECT(to_string(b.data()) ==
                    std::string(v.data(), v.size()));
                v.push_back(n+1);
            }
        }
    }

    void
    testPausation1()
    {
        for(int i = 0; i < 2; ++i)
        {
            boost::asio::io_service ios;
            stream<test::stream> ws{ios, ios_};
            if(i == 0)
                launchEchoServer(ws.next_layer().remote());
            else
                launchEchoServerAsync(ws.next_layer().remote());
            ws.handshake("localhost", "/");

            // Make remote send a text message with bad utf8.
            ws.binary(true);
            put(ws.next_layer().buffer(), cbuf(
                0x03, 0xea, 0xf0, 0x28, 0x8c, 0xbc));
            multi_buffer b;
            std::size_t count = 0;
            // Read text message with bad utf8.
            // Causes a close to be sent, blocking writes.
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    // Read should fail with protocol error
                    ++count;
                    BEAST_EXPECTS(
                        ec == error::failed, ec.message());
                    // Reads after failure are aborted
                    ws.async_read(b,
                        [&](error_code ec, std::size_t)
                        {
                            ++count;
                            BEAST_EXPECTS(ec == boost::asio::
                                error::operation_aborted,
                                    ec.message());
                        });
                });
            // Run until the read_op writes a close frame.
            while(! ws.wr_block_)
                ios.run_one();
            // Write a text message, leaving
            // the write_op suspended as a pausation.
            ws.async_write(sbuf("Hello"),
                [&](error_code ec)
                {
                    ++count;
                    // Send is canceled because close received.
                    BEAST_EXPECTS(ec == boost::asio::
                        error::operation_aborted,
                            ec.message());
                    // Writes after close are aborted.
                    ws.async_write(sbuf("World"),
                        [&](error_code ec)
                        {
                            ++count;
                            BEAST_EXPECTS(ec == boost::asio::
                                error::operation_aborted,
                                    ec.message());
                        });
                });
            // Run until all completions are delivered.
            while(! ios.stopped())
                ios.run_one();
            BEAST_EXPECT(count == 4);
        }
    }

    void
    testPausation2(endpoint_type const& ep)
    {
        boost::asio::io_service ios;
        stream<socket_type> ws(ios);
        ws.next_layer().connect(ep);
        ws.handshake("localhost", "/");

        // Cause close to be received
        ws.binary(true);
        ws.write(sbuf("CLOSE"));
        multi_buffer b;
        std::size_t count = 0;
        // Read a close frame.
        // Sends a close frame, blocking writes.
        ws.async_read(b,
            [&](error_code ec, std::size_t)
            {
                // Read should complete with error::closed
                ++count;
                BEAST_EXPECTS(ec == error::closed,
                    ec.message());
                // Pings after a close are aborted
                ws.async_ping("",
                    [&](error_code ec)
                    {
                        ++count;
                        BEAST_EXPECTS(ec == boost::asio::
                            error::operation_aborted,
                                ec.message());
                    });
            });
        if(! BEAST_EXPECT(run_until(ios, 100,
                [&]{ return ws.wr_close_; })))
            return;
        // Try to ping
        ws.async_ping("payload",
            [&](error_code ec)
            {
                // Pings after a close are aborted
                ++count;
                BEAST_EXPECTS(ec == boost::asio::
                    error::operation_aborted,
                        ec.message());
                // Subsequent calls to close are aborted
                ws.async_close({},
                    [&](error_code ec)
                    {
                        ++count;
                        BEAST_EXPECTS(ec == boost::asio::
                            error::operation_aborted,
                                ec.message());
                    });
            });
        static std::size_t constexpr limit = 100;
        std::size_t n;
        for(n = 0; n < limit; ++n)
        {
            if(count >= 4)
                break;
            ios.run_one();
        }
        BEAST_EXPECT(n < limit);
        ios.run();
    }

    void
    testPausation3(endpoint_type const& ep)
    {
        boost::asio::io_service ios;
        stream<socket_type> ws(ios);
        ws.next_layer().connect(ep);
        ws.handshake("localhost", "/");

        // Cause close to be received
        ws.binary(true);
        ws.write(sbuf("CLOSE"));
        multi_buffer b;
        std::size_t count = 0;
        ws.async_read(b,
            [&](error_code ec, std::size_t)
            {
                ++count;
                BEAST_EXPECTS(ec == error::closed,
                    ec.message());
            });
        while(! ws.wr_block_)
            ios.run_one();
        // try to close
        ws.async_close("payload",
            [&](error_code ec)
            {
                ++count;
                BEAST_EXPECTS(ec == boost::asio::
                    error::operation_aborted,
                        ec.message());
            });
        static std::size_t constexpr limit = 100;
        std::size_t n;
        for(n = 0; n < limit; ++n)
        {
            if(count >= 2)
                break;
            ios.run_one();
        }
        BEAST_EXPECT(n < limit);
        ios.run();
    }

    /*
        https://github.com/boostorg/beast/issues/300

        Write a message as two individual frames
    */
    void
    testWriteFrames()
    {
        for(int i = 0; i < 2; ++i)
        {
            error_code ec;
            stream<test::stream> ws{ios_};
            if(i == 0)
                launchEchoServer(ws.next_layer().remote());
            else
                launchEchoServerAsync(ws.next_layer().remote());
            ws.handshake("localhost", "/", ec);
            if(! BEAST_EXPECTS(! ec, ec.message()))
                return;
            ws.write_some(false, sbuf("u"));
            ws.write_some(true, sbuf("v"));
            multi_buffer b;
            ws.read(b, ec);
            BEAST_EXPECTS(! ec, ec.message());
        }
    }

    void
    testAsyncWriteFrame()
    {
        for(int i = 0; i < 2; ++i)
        {
            for(;;)
            {
                error_code ec;
                boost::asio::io_service ios;
                stream<test::stream> ws{ios, ios_};
                if(i == 0)
                    launchEchoServer(ws.next_layer().remote());
                else
                    launchEchoServerAsync(ws.next_layer().remote());
                ws.handshake("localhost", "/", ec);
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    break;
                ws.async_write_some(false,
                    boost::asio::null_buffers{},
                    [&](error_code)
                    {
                        fail();
                    });
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    break;
                //
                // Destruction of the io_service will cause destruction
                // of the write_some_op without invoking the final handler.
                //
                break;
            }
        }
    }

    //--------------------------------------------------------------------------

    template<class Wrap, class Launch>
    void
    testStream(
        Wrap const& c,
        permessage_deflate const& pmd,
        Launch const& launch)
    {
        using boost::asio::buffer;

        // send empty message
        doTest(c, pmd, launch, [&](ws_stream_type& ws)
        {
            ws.text(true);
            c.write(ws, boost::asio::null_buffers{});
            multi_buffer b;
            c.read(ws, b);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(b.size() == 0);
        });

        // send message
        doTest(c, pmd, launch, [&](ws_stream_type& ws)
        {
            ws.auto_fragment(false);
            ws.binary(false);
            c.write(ws, sbuf("Hello"));
            multi_buffer b;
            c.read(ws, b);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(to_string(b.data()) == "Hello");
        });

        // read_some
        doTest(c, pmd, launch, [&](ws_stream_type& ws)
        {
            c.write(ws, sbuf("Hello"));
            char buf[10];
            auto const bytes_written =
                c.read_some(ws, buffer(buf, sizeof(buf)));
            BEAST_EXPECT(bytes_written > 0);
            buf[bytes_written] = 0;
            BEAST_EXPECT(
                string_view(buf).substr(0, bytes_written) ==
                string_view("Hello").substr(0, bytes_written));
        });

        // close, no payload
        doTest(c, pmd, launch, [&](ws_stream_type& ws)
        {
            c.close(ws, {});
        });

        // close with code
        doTest(c, pmd, launch, [&](ws_stream_type& ws)
        {
            c.close(ws, close_code::going_away);
        });

        // send ping and message
        doTest(c, pmd, launch, [&](ws_stream_type& ws)
        {
            bool once = false;
            auto cb =
                [&](frame_type kind, string_view s)
                {
                    BEAST_EXPECT(kind == frame_type::pong);
                    BEAST_EXPECT(! once);
                    once = true;
                    BEAST_EXPECT(s == "");
                };
            ws.control_callback(cb);
            c.ping(ws, "");
            ws.binary(true);
            c.write(ws, sbuf("Hello"));
            multi_buffer b;
            c.read(ws, b);
            BEAST_EXPECT(once);
            BEAST_EXPECT(ws.got_binary());
            BEAST_EXPECT(to_string(b.data()) == "Hello");
        });

        // send ping and fragmented message
        doTest(c, pmd, launch, [&](ws_stream_type& ws)
        {
            bool once = false;
            auto cb =
                [&](frame_type kind, string_view s)
                {
                    BEAST_EXPECT(kind == frame_type::pong);
                    BEAST_EXPECT(! once);
                    once = true;
                    BEAST_EXPECT(s == "payload");
                };
            ws.control_callback(cb);
            ws.ping("payload");
            c.write_some(ws, false, sbuf("Hello, "));
            c.write_some(ws, false, sbuf(""));
            c.write_some(ws, true, sbuf("World!"));
            multi_buffer b;
            c.read(ws, b);
            BEAST_EXPECT(once);
            BEAST_EXPECT(to_string(b.data()) == "Hello, World!");
            ws.control_callback();
        });

        // send pong
        doTest(c, pmd, launch, [&](ws_stream_type& ws)
        {
            c.pong(ws, "");
        });

        // send auto fragmented message
        doTest(c, pmd, launch, [&](ws_stream_type& ws)
        {
            ws.auto_fragment(true);
            ws.write_buffer_size(8);
            c.write(ws, sbuf("Now is the time for all good men"));
            multi_buffer b;
            c.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == "Now is the time for all good men");
        });

        // send message with write buffer limit
        doTest(c, pmd, launch, [&](ws_stream_type& ws)
        {
            std::string s(2000, '*');
            ws.write_buffer_size(1200);
            c.write(ws, buffer(s.data(), s.size()));
            multi_buffer b;
            c.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // unexpected cont
        doTest(c, pmd, launch, [&](ws_stream_type& ws)
        {
            c.write_raw(ws, cbuf(
                0x80, 0x80, 0xff, 0xff, 0xff, 0xff));
            doCloseTest(c, ws, close_code::protocol_error);
        });

        // invalid fixed frame header
        doTest(c, pmd, launch, [&](ws_stream_type& ws)
        {
            c.write_raw(ws, cbuf(
                0x8f, 0x80, 0xff, 0xff, 0xff, 0xff));
            doCloseTest(c, ws, close_code::protocol_error);
        });

        if(! pmd.client_enable)
        {
            // expected cont
            doTest(c, pmd, launch, [&](ws_stream_type& ws)
            {
                c.write_some(ws, false, boost::asio::null_buffers{});
                c.write_raw(ws, cbuf(
                    0x81, 0x80, 0xff, 0xff, 0xff, 0xff));
                doCloseTest(c, ws, close_code::protocol_error);
            });

            // message size above 2^64
            doTest(c, pmd, launch, [&](ws_stream_type& ws)
            {
                c.write_some(ws, false, cbuf(0x00));
                c.write_raw(ws, cbuf(
                    0x80, 0xff, 0xff, 0xff, 0xff, 0xff,
                    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff));
                doCloseTest(c, ws, close_code::too_big);
            });

            /*
            // message size exceeds max
            doTest(c, pmd, launch, [&](ws_stream_type& ws)
            {
                // VFALCO This was never implemented correctly
                ws.read_message_max(1);
                c.write(ws, cbuf(0x81, 0x02, '*', '*'));
                doCloseTest(c, ws, close_code::too_big);
            });
            */
        }

        // receive ping
        doTest(c, pmd, launch, [&](ws_stream_type& ws)
        {
            put(ws.next_layer().buffer(), cbuf(
                0x89, 0x00));
            bool invoked = false;
            auto cb = [&](frame_type kind, string_view)
            {
                BEAST_EXPECT(! invoked);
                BEAST_EXPECT(kind == frame_type::ping);
                invoked = true;
            };
            ws.control_callback(cb);
            c.write(ws, sbuf("Hello"));
            multi_buffer b;
            c.read(ws, b);
            BEAST_EXPECT(invoked);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(to_string(b.data()) == "Hello");
        });

        // receive ping
        doTest(c, pmd, launch, [&](ws_stream_type& ws)
        {
            put(ws.next_layer().buffer(), cbuf(
                0x88, 0x00));
            bool invoked = false;
            auto cb = [&](frame_type kind, string_view)
            {
                BEAST_EXPECT(! invoked);
                BEAST_EXPECT(kind == frame_type::close);
                invoked = true;
            };
            ws.control_callback(cb);
            c.write(ws, sbuf("Hello"));
            doCloseTest(c, ws, close_code::none);
        });

        // receive bad utf8
        doTest(c, pmd, launch, [&](ws_stream_type& ws)
        {
            put(ws.next_layer().buffer(), cbuf(
                0x81, 0x06, 0x03, 0xea, 0xf0, 0x28, 0x8c, 0xbc));
            doFailTest(c, ws, error::failed);
        });

        // receive bad close
        doTest(c, pmd, launch, [&](ws_stream_type& ws)
        {
            put(ws.next_layer().buffer(), cbuf(
                0x88, 0x02, 0x03, 0xed));
            doFailTest(c, ws, error::failed);
        });
    }

    //--------------------------------------------------------------------------

    void
    run() override
    {
        BOOST_STATIC_ASSERT(std::is_constructible<
            stream<socket_type>, boost::asio::io_service&>::value);

        BOOST_STATIC_ASSERT(std::is_move_constructible<
            stream<socket_type>>::value);

        BOOST_STATIC_ASSERT(std::is_move_assignable<
            stream<socket_type>>::value);

        BOOST_STATIC_ASSERT(std::is_constructible<
            stream<socket_type&>, socket_type&>::value);

        BOOST_STATIC_ASSERT(std::is_move_constructible<
            stream<socket_type&>>::value);

        BOOST_STATIC_ASSERT(! std::is_move_assignable<
            stream<socket_type&>>::value);

        log << "sizeof(websocket::stream) == " <<
            sizeof(websocket::stream<test::stream&>) << std::endl;

        testAccept();
        testClose();
        testHandshake();
        testPing();
        testRead();

        permessage_deflate pmd;
        pmd.client_enable = false;
        pmd.server_enable = false;

        testOptions();
        testPausation1();
        testWriteFrames();
        testAsyncWriteFrame();

        // Legacy tests use actual TCP/IP sockets
        // VFALCO: Rewrite to use test::stream and
        //         remote <boost/asio/ip/tcp.hpp>
        {
            auto const any = endpoint_type{
                address_type::from_string("127.0.0.1"), 0};
            error_code ec;
            ::websocket::sync_echo_server server{nullptr};
            server.set_option(pmd);
            server.open(any, ec);
            BEAST_EXPECTS(! ec, ec.message());
            auto const ep = server.local_endpoint();
            testPausation2(ep);
            testPausation3(ep);
        }

        auto const doClientTests =
            [this](permessage_deflate const& pmd)
            {
                testStream(SyncClient{}, pmd,
                    [&](test::stream stream)
                    {
                        launchEchoServer(std::move(stream));
                    });
#if 0
                // This causes false positives on ASAN
                testStream(SyncClient{}, pmd,
                    [&](test::stream stream)
                    {
                        launchEchoServerAsync(std::move(stream));
                    });
#endif
                yield_to(
                    [&](yield_context yield)
                    {
                        testStream(AsyncClient{yield}, pmd,
                            [&](test::stream stream)
                            {
                                launchEchoServer(std::move(stream));
                            });
                    });
                yield_to(
                    [&](yield_context yield)
                    {
                        testStream(AsyncClient{yield}, pmd,
                            [&](test::stream stream)
                            {
                                launchEchoServerAsync(std::move(stream));
                            });
                    });
            };

        pmd.client_enable = false;
        pmd.server_enable = false;
        doClientTests(pmd);

        pmd.client_enable = true;
        pmd.server_enable = true;
        pmd.client_max_window_bits = 10;
        pmd.client_no_context_takeover = false;
        pmd.compLevel = 1;
        pmd.memLevel = 1;
        doClientTests(pmd);

        pmd.client_enable = true;
        pmd.server_enable = true;
        pmd.client_max_window_bits = 10;
        pmd.client_no_context_takeover = true;
        pmd.compLevel = 1;
        pmd.memLevel = 1;
        doClientTests(pmd);
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,stream);

} // websocket
} // beast
} // boost
