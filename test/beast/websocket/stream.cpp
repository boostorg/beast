//
// Copyright (w) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/websocket/stream.hpp>

#include <boost/beast/core/ostream.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/test/stream.hpp>
#include <boost/beast/test/yield_to.hpp>
#include <boost/beast/unit_test/suite.hpp>
#include <boost/optional.hpp>
#include <memory>
#include <random>

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

    using ws_type =
        websocket::stream<test::stream&>;

    //--------------------------------------------------------------------------

    enum class kind
    {
        sync,
        async,
        async_client
    };

    class echo_server
    {
        enum
        {
            buf_size = 20000
        };

        std::ostream& log_;
        boost::asio::io_service ios_;
        boost::optional<
            boost::asio::io_service::work> work_;
        static_buffer<buf_size> buffer_;
        test::stream ts_;
        std::thread t_;
        websocket::stream<test::stream&> ws_;

    public:
        explicit
        echo_server(
            std::ostream& log,
            kind k = kind::sync)
            : log_(log)
            , work_(ios_)
            , ts_(ios_)
            , ws_(ts_)
        {
            permessage_deflate pmd;
            pmd.client_enable = true;
            pmd.server_enable = true;
            ws_.set_option(pmd);

            switch(k)
            {
            case kind::sync:
                t_ = std::thread{[&]{ do_sync(); }};
                break;

            case kind::async:
                t_ = std::thread{[&]{ ios_.run(); }};
                do_accept();
                break;

            case kind::async_client:
                t_ = std::thread{[&]{ ios_.run(); }};
                break;
            }
        }

        ~echo_server()
        {
            work_ = boost::none;
            t_.join();
        }

        test::stream&
        stream()
        {
            return ts_;
        }

        void
        async_handshake()
        {
            ws_.async_handshake("localhost", "/",
                std::bind(
                    &echo_server::on_handshake,
                    this,
                    std::placeholders::_1));
        }

        void
        async_close()
        {
            ios_.post(
            [&]
            {
                ws_.async_close({},
                    std::bind(
                        &echo_server::on_close,
                        this,
                        std::placeholders::_1));
            });
        }

    private:
        void
        do_sync()
        {
            try
            {
                ws_.accept();
                for(;;)
                {
                    ws_.read(buffer_);
                    ws_.text(ws_.got_text());
                    ws_.write(buffer_.data());
                    buffer_.consume(buffer_.size());
                }
            }
            catch(system_error const& se)
            {
                boost::ignore_unused(se);
#if 0
                if( se.code() != error::closed &&
                    se.code() != error::failed &&
                    se.code() != boost::asio::error::eof)
                    log_ << "echo_server: " << se.code().message() << std::endl;
#endif
            }
            catch(std::exception const& e)
            {
                log_ << "echo_server: " << e.what() << std::endl;
            }
        }

        void
        do_accept()
        {
            ws_.async_accept(std::bind(
                &echo_server::on_accept,
                this,
                std::placeholders::_1));
        }

        void
        on_handshake(error_code ec)
        {
            if(ec)
                return fail(ec);
            do_read();
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
                std::bind(
                    &echo_server::on_read,
                    this,
                    std::placeholders::_1));
        }

        void
        on_read(error_code ec)
        {
            if(ec)
                return fail(ec);
            ws_.text(ws_.got_text());
            ws_.async_write(buffer_.data(),
                std::bind(
                    &echo_server::on_write,
                    this,
                    std::placeholders::_1));
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
        on_close(error_code ec)
        {
            if(ec)
                return fail(ec);
        }

        void
        fail(error_code ec)
        {
            boost::ignore_unused(ec);
#if 0
            if( ec != error::closed &&
                ec != error::failed &&
                ec != boost::asio::error::eof)
                log_ <<
                    "echo_server_async: " <<
                    ec.message() <<
                    std::endl;
#endif
        }
    };

    //--------------------------------------------------------------------------

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
                ts.close();
                break;
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == test::error::fail_error,
                    se.code().message());
            }
            catch(std::exception const& e)
            {
                fail(e.what(), __FILE__, __LINE__);
            }
            ts.close();
            continue;
        }
        BEAST_EXPECT(n < limit);
    }

    template<class Test>
    void
    doTest(
        permessage_deflate const& pmd,
        Test const& f)
    {
        // This number has to be high for the
        // test that writes the large buffer.
        static std::size_t constexpr limit = 1000;

        for(int i = 0; i < 2; ++i)
        {
            std::size_t n;
            for(n = 0; n <= limit; ++n)
            {
                test::fail_counter fc{n};
                test::stream ts{ios_, fc};
                ws_type ws{ts};
                ws.set_option(pmd);

                echo_server es{log, i==1 ?
                    kind::async : kind::sync};
                error_code ec;
                ws.next_layer().connect(es.stream());
                ws.handshake("localhost", "/", ec);
                if(ec)
                {
                    ts.close();
                    if( ! BEAST_EXPECTS(
                            ec == test::error::fail_error,
                            ec.message()))
                        BOOST_THROW_EXCEPTION(system_error{ec});
                    continue;
                }
                try
                {
                    f(ws);
                    ts.close();
                    break;
                }
                catch(system_error const& se)
                {
                    BEAST_EXPECTS(
                        se.code() == test::error::fail_error,
                        se.code().message());
                }
                catch(std::exception const& e)
                {
                    fail(e.what(), __FILE__, __LINE__);
                }
                ts.close();
                continue;
            }
            BEAST_EXPECT(n < limit);
        }
    }

    template<class Wrap>
    void
    doCloseTest(
        Wrap const& w,
        ws_type& ws,
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
        ws_type& ws,
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
    //
    // Accept
    //
    //--------------------------------------------------------------------------

    template<class Wrap>
    void
    doTestAccept(Wrap const& w)
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
            auto tr = connect(ws.next_layer());
            ts.append(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n");
            ts.read_size(20);
            w.accept(ws);
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
            auto tr = connect(ws.next_layer());
            try
            {
                w.accept(ws);
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
            auto tr = connect(ws.next_layer());
            ts.append(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n");
            ts.read_size(20);
            bool called = false;
            w.accept_ex(ws, res_decorator{called});
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
            auto tr = connect(ws.next_layer());
            try
            {
                bool called = false;
                w.accept_ex(ws, res_decorator{called});
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
            auto tr = connect(ws.next_layer());
            w.accept(ws, sbuf(
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
            auto tr = connect(ws.next_layer());
            try
            {
                w.accept(ws, boost::asio::buffer(
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
            auto tr = connect(ws.next_layer());
            bool called = false;
            w.accept_ex(ws, sbuf(
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
            auto tr = connect(ws.next_layer());
            try
            {
                bool called = false;
                w.accept_ex(ws, boost::asio::buffer(
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
            auto tr = connect(ws.next_layer());
            ts.append(
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n");
            ts.read_size(16);
            w.accept(ws, sbuf(
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
            auto tr = connect(ws.next_layer());
            try
            {
                w.accept(ws, sbuf(
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
            auto tr = connect(ws.next_layer());
            ts.append(
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n");
            ts.read_size(16);
            bool called = false;
            w.accept_ex(ws, sbuf(
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
            auto tr = connect(ws.next_layer());
            try
            {
                bool called = false;
                w.accept_ex(ws, sbuf(
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
            auto tr = connect(ws.next_layer());
            request_type req;
            req.method(http::verb::get);
            req.target("/");
            req.version = 11;
            req.insert(http::field::host, "localhost");
            req.insert(http::field::upgrade, "websocket");
            req.insert(http::field::connection, "upgrade");
            req.insert(http::field::sec_websocket_key, "dGhlIHNhbXBsZSBub25jZQ==");
            req.insert(http::field::sec_websocket_version, "13");
            w.accept(ws, req);
        });

        // request in message, decorator
        doTestLoop([&](test::stream& ts)
        {
            stream<test::stream&> ws{ts};
            auto tr = connect(ws.next_layer());
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
            w.accept_ex(ws, req,
                res_decorator{called});
            BEAST_EXPECT(called);
        });

        // request in message, close frame in stream
        doTestLoop([&](test::stream& ts)
        {
            stream<test::stream&> ws{ts};
            auto tr = connect(ws.next_layer());
            request_type req;
            req.method(http::verb::get);
            req.target("/");
            req.version = 11;
            req.insert(http::field::host, "localhost");
            req.insert(http::field::upgrade, "websocket");
            req.insert(http::field::connection, "upgrade");
            req.insert(http::field::sec_websocket_key, "dGhlIHNhbXBsZSBub25jZQ==");
            req.insert(http::field::sec_websocket_version, "13");
            ts.append("\x88\x82\xff\xff\xff\xff\xfc\x17");
            w.accept(ws, req);
            try
            {
                static_buffer<1> b;
                w.read(ws, b);
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
            auto tr = connect(ws.next_layer());
            ts.append(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n");
            ts.read_size(20);
            try
            {
                w.accept(ws);
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
            auto tr = connect(ws.next_layer());
            tr.close();
            try
            {
                w.accept(ws);
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
                auto tr = connect(ws.next_layer());
                ws.next_layer().append(
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

        // normal close
        doTest(pmd, [&](ws_type& ws)
        {
            w.close(ws, {});
        });

        // double close
        {
            echo_server es{log};
            stream<test::stream> ws{ios_};
            ws.next_layer().connect(es.stream());
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
        doTest(pmd, [&](ws_type& ws)
        {
            ws.next_layer().append("\x81\x01\x2a");
            w.close(ws, {});
        });

        // drain a big message after close
        {
            std::string s;
            s = "\x81\x7e\x10\x01";
            s.append(4097, '*');
            doTest(pmd, [&](ws_type& ws)
            {
                ws.next_layer().append(s);
                w.close(ws, {});
            });
        }

        // drain a ping after close
        doTest(pmd, [&](ws_type& ws)
        {
            ws.next_layer().append("\x89\x01*");
            w.close(ws, {});
        });

        // drain invalid message frame after close
        {
            echo_server es{log};
            stream<test::stream> ws{ios_};
            ws.next_layer().connect(es.stream());
            w.handshake(ws, "localhost", "/");
            ws.next_layer().append("\x81\x81\xff\xff\xff\xff*");
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
            echo_server es{log};
            stream<test::stream> ws{ios_};
            ws.next_layer().connect(es.stream());
            w.handshake(ws, "localhost", "/");
            ws.next_layer().append("\x88\x01*");
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
        doTest(pmd, [&](ws_type& ws)
        {
            ws.next_layer().append("\x81\x02**");
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

        // suspend on write
        {
            echo_server es{log};
            error_code ec;
            boost::asio::io_service ios;
            stream<test::stream> ws{ios};
            ws.next_layer().connect(es.stream());
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
            echo_server es{log};
            error_code ec;
            boost::asio::io_service ios;
            stream<test::stream> ws{ios};
            ws.next_layer().connect(es.stream());
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

    template<class Wrap>
    void
    doTestHandshake(Wrap const& w)
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
            echo_server es{log};
            ws_type ws{ts};
            ws.next_layer().connect(es.stream());
            try
            {
                w.handshake(ws, "localhost", "/");
            }
            catch(...)
            {
                ts.close();
                throw;
            }
            ts.close();
        });

        // handshake, response
        doTestLoop([&](test::stream& ts)
        {
            echo_server es{log};
            ws_type ws{ts};
            ws.next_layer().connect(es.stream());
            response_type res;
            try
            {
                w.handshake(ws, res, "localhost", "/");
                // VFALCO validate res?
            }
            catch(...)
            {
                ts.close();
                throw;
            }
            ts.close();
        });

        // handshake, decorator
        doTestLoop([&](test::stream& ts)
        {
            echo_server es{log};
            ws_type ws{ts};
            ws.next_layer().connect(es.stream());
            bool called = false;
            try
            {
                w.handshake_ex(ws, "localhost", "/",
                    req_decorator{called});
                BEAST_EXPECT(called);
            }
            catch(...)
            {
                ts.close();
                throw;
            }
            ts.close();
        });

        // handshake, response, decorator
        doTestLoop([&](test::stream& ts)
        {
            echo_server es{log};
            ws_type ws{ts};
            ws.next_layer().connect(es.stream());
            bool called = false;
            response_type res;
            try
            {
                w.handshake_ex(ws, res, "localhost", "/",
                    req_decorator{called});
                // VFALCO validate res?
                BEAST_EXPECT(called);
            }
            catch(...)
            {
                ts.close();
                throw;
            }
            ts.close();
        });
    }

    void
    testHandshake()
    {
        doTestHandshake(SyncClient{});

        yield_to([&](yield_context yield)
        {
            doTestHandshake(AsyncClient{yield});
        });

        auto const check =
        [&](std::string const& s)
        {
            stream<test::stream> ws{ios_};
            auto tr = connect(ws.next_layer());
            ws.next_layer().append(s);
            tr.close();
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
        permessage_deflate pmd;
        pmd.client_enable = false;
        pmd.server_enable = false;

        // ping
        doTest(pmd, [&](ws_type& ws)
        {
            w.ping(ws, {});
        });

        // pong
        doTest(pmd, [&](ws_type& ws)
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
            echo_server es{log};
            error_code ec;
            boost::asio::io_service ios;
            stream<test::stream> ws{ios};
            ws.next_layer().connect(es.stream());
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

    static
    std::string const&
    random_string()
    {
        static std::string const s = []
        {
            std::size_t constexpr N = 16384;
            std::mt19937 mt{1};
            std::string tmp;
            tmp.reserve(N);
            for(std::size_t i = 0; i < N; ++ i)
                tmp.push_back(static_cast<char>(
                    std::uniform_int_distribution<
                        unsigned>{0, 255}(mt)));
            return tmp;
        }();
        return s;
    }

    template<class Wrap>
    void
    doTestRead(Wrap const& w)
    {
        permessage_deflate pmd;
        pmd.client_enable = false;
        pmd.server_enable = false;

        // Read close frames
        {
            // VFALCO What about asynchronous??

            auto const check =
            [&](error_code ev, string_view s)
            {
                echo_server es{log};
                stream<test::stream> ws{ios_};
                ws.next_layer().connect(es.stream());
                w.handshake(ws, "localhost", "/");
                ws.next_layer().append(s);
                static_buffer<1> b;
                error_code ec;
                try
                {
                    w.read(ws, b);
                    fail("", __FILE__, __LINE__);
                }
                catch(system_error const& se)
                {
                    BEAST_EXPECTS(se.code() == ev,
                        se.code().message());
                }
                ws.next_layer().close();
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

        pmd.client_enable = true;
        pmd.server_enable = true;

        // invalid inflate block
        doTest(pmd, [&](ws_type& ws)
        {
            auto const& s = random_string();
            ws.binary(true);
            ws.next_layer().append(
                "\xc2\x40" + s.substr(0, 64));
            flat_buffer b;
            try
            {
                w.read(ws, b);
            }
            catch(system_error const& se)
            {
                if(se.code() == test::error::fail_error)
                    throw;
                BEAST_EXPECTS(se.code().category() ==
                    zlib::detail::get_error_category(),
                        se.code().message());
            }
            catch(...)
            {
                throw;
            }
        });
    }

    void
    testRead()
    {
        doTestRead(SyncClient{});

        yield_to([&](yield_context yield)
        {
            doTestRead(AsyncClient{yield});
        });

        // Read close frames
        {
            auto const check =
            [&](error_code ev, string_view s)
            {
                echo_server es{log};
                stream<test::stream> ws{ios_};
                ws.next_layer().connect(es.stream());
                ws.handshake("localhost", "/");
                ws.next_layer().append(s);
                static_buffer<1> b;
                error_code ec;
                ws.read(b, ec);
                BEAST_EXPECTS(ec == ev, ec.message());
                ws.next_layer().close();
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
    //
    // Write
    //
    //--------------------------------------------------------------------------

    template<class Wrap>
    void
    doTestWrite(Wrap const& w)
    {
        permessage_deflate pmd;
        pmd.client_enable = false;
        pmd.server_enable = false;

        // continuation
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = "Hello";
            std::size_t const chop = 3;
            BOOST_ASSERT(chop < s.size());
            w.write_some(ws, false,
                boost::asio::buffer(s.data(), chop));
            w.write_some(ws, true, boost::asio::buffer(
                s.data() + chop, s.size() - chop));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // mask
        doTest(pmd, [&](ws_type& ws)
        {
            ws.auto_fragment(false);
            std::string const s = "Hello";
            w.write(ws, boost::asio::buffer(s));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // mask (large)
        doTest(pmd, [&](ws_type& ws)
        {
            ws.auto_fragment(false);
            ws.write_buffer_size(16);
            std::string const s(32, '*');
            w.write(ws, boost::asio::buffer(s));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // mask, autofrag
        doTest(pmd, [&](ws_type& ws)
        {
            ws.auto_fragment(true);
            std::string const s(16384, '*');
            w.write(ws, boost::asio::buffer(s));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // nomask
        doTestLoop([&](test::stream& ts)
        {
            echo_server es{log, kind::async_client};
            ws_type ws{ts};
            ws.next_layer().connect(es.stream());
            try
            {
                es.async_handshake();
                w.accept(ws);
                ws.auto_fragment(false);
                std::string const s = "Hello";
                w.write(ws, boost::asio::buffer(s));
                flat_buffer b;
                w.read(ws, b);
                BEAST_EXPECT(to_string(b.data()) == s);
                w.close(ws, {});
            }
            catch(...)
            {
                ts.close();
                throw;
            }
            ts.close();
        });

        // nomask, autofrag
        doTestLoop([&](test::stream& ts)
        {
            echo_server es{log, kind::async_client};
            ws_type ws{ts};
            ws.next_layer().connect(es.stream());
            try
            {
                es.async_handshake();
                w.accept(ws);
                ws.auto_fragment(true);
                std::string const s(16384, '*');
                w.write(ws, boost::asio::buffer(s));
                flat_buffer b;
                w.read(ws, b);
                BEAST_EXPECT(to_string(b.data()) == s);
                w.close(ws, {});
            }
            catch(...)
            {
                ts.close();
                throw;
            }
            ts.close();
        });

        pmd.client_enable = true;
        pmd.server_enable = true;

        // deflate
        doTest(pmd, [&](ws_type& ws)
        {
            auto const& s = random_string();
            ws.binary(true);
            w.write(ws, boost::asio::buffer(s));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // deflate, continuation
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = "Hello";
            std::size_t const chop = 3;
            BOOST_ASSERT(chop < s.size());
            // This call should produce no
            // output due to compression latency.
            w.write_some(ws, false,
                boost::asio::buffer(s.data(), chop));
            w.write_some(ws, true, boost::asio::buffer(
                s.data() + chop, s.size() - chop));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // deflate, no context takeover
        pmd.client_no_context_takeover = true;
        doTest(pmd, [&](ws_type& ws)
        {
            auto const& s = random_string();
            ws.binary(true);
            w.write(ws, boost::asio::buffer(s));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });
    }

    void
    testWrite()
    {
        doTestWrite(SyncClient{});

        yield_to([&](yield_context yield)
        {
            doTestWrite(AsyncClient{yield});
        });

        // already closed
        {
            stream<test::stream> ws{ios_};
            error_code ec;
            ws.write(sbuf(""), ec);
            BEAST_EXPECTS(
                ec == boost::asio::error::operation_aborted,
                ec.message());
        }

        // async, already closed
        {
            boost::asio::io_service ios;
            stream<test::stream> ws{ios};
            ws.async_write(sbuf(""),
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
            echo_server es{log};
            error_code ec;
            boost::asio::io_service ios;
            stream<test::stream> ws{ios};
            ws.next_layer().connect(es.stream());
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
            ws.async_write(sbuf("*"),
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

        // suspend on write, nomask, frag
        {
            echo_server es{log, kind::async_client};
            error_code ec;
            boost::asio::io_service ios;
            stream<test::stream> ws{ios};
            ws.next_layer().connect(es.stream());
            es.async_handshake();
            ws.accept(ec);
            BEAST_EXPECTS(! ec, ec.message());
            std::size_t count = 0;
            std::string const s(16384, '*');
            ws.auto_fragment(true);
            ws.async_write(boost::asio::buffer(s),
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
                    BEAST_EXPECTS(! ec, ec.message());
                });
            ios.run();
            ios.reset();
            BEAST_EXPECT(count == 2);
            flat_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                    BEAST_EXPECT(to_string(b.data()) == s);
                    ws.async_close({},
                        [&](error_code ec)
                        {
                            ++count;
                            BEAST_EXPECTS(! ec, ec.message());
                        });
                });
            ios.run();
            BEAST_EXPECT(count == 4);
        }

        // suspend on write, mask, frag
        {
            echo_server es{log, kind::async};
            error_code ec;
            boost::asio::io_service ios;
            stream<test::stream> ws{ios};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/", ec);
            BEAST_EXPECTS(! ec, ec.message());
            std::size_t count = 0;
            std::string const s(16384, '*');
            ws.auto_fragment(true);
            ws.async_write(boost::asio::buffer(s),
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
                    BEAST_EXPECTS(! ec, ec.message());
                });
            ios.run();
            ios.reset();
            BEAST_EXPECT(count == 2);
            flat_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                    BEAST_EXPECT(to_string(b.data()) == s);
                    ws.async_close({},
                        [&](error_code ec)
                        {
                            ++count;
                            BEAST_EXPECTS(! ec, ec.message());
                        });
                });
            ios.run();
            BEAST_EXPECT(count == 4);
        }

        // suspend on write, deflate
        {
            echo_server es{log, kind::async};
            error_code ec;
            boost::asio::io_service ios;
            stream<test::stream> ws{ios};
            {
                permessage_deflate pmd;
                pmd.client_enable = true;
                ws.set_option(pmd);
            }
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/", ec);
            BEAST_EXPECTS(! ec, ec.message());
            std::size_t count = 0;
            auto const& s = random_string();
            ws.binary(true);
            ws.async_write(boost::asio::buffer(s),
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
                    BEAST_EXPECTS(! ec, ec.message());
                });
            ios.run();
            ios.reset();
            BEAST_EXPECT(count == 2);
            flat_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                    BEAST_EXPECT(to_string(b.data()) == s);
                    ws.async_close({},
                        [&](error_code ec)
                        {
                            ++count;
                            BEAST_EXPECTS(! ec, ec.message());
                        });
                });
            ios.run();
            BEAST_EXPECT(count == 4);
        }
    }

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

    void
    testPausation1()
    {
        for(int i = 0; i < 2; ++i )
        {
            echo_server es{log, i==1 ?
                kind::async : kind::sync};
            boost::asio::io_service ios;
            stream<test::stream> ws{ios};
            ws.next_layer().connect(es.stream());
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
    testPausation2()
    {
        echo_server es{log, kind::async};
        boost::asio::io_service ios;
        stream<test::stream> ws{ios};
        ws.next_layer().connect(es.stream());
        ws.handshake("localhost", "/");

        // Cause close to be received
        es.async_close();
            
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
    testPausation3()
    {
        echo_server es{log, kind::async};
        boost::asio::io_service ios;
        stream<test::stream> ws{ios};
        ws.next_layer().connect(es.stream());
        ws.handshake("localhost", "/");

        // Cause close to be received
        es.async_close();

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
    testIssue300()
    {
        for(int i = 0; i < 2; ++i )
        {
            echo_server es{log, i==1 ?
                kind::async : kind::sync};
            boost::asio::io_service ios;
            stream<test::stream> ws{ios};
            ws.next_layer().connect(es.stream());

            error_code ec;
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
                echo_server es{log, i==1 ?
                    kind::async : kind::sync};
                boost::asio::io_service ios;
                stream<test::stream> ws{ios};
                ws.next_layer().connect(es.stream());

                error_code ec;
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

    template<class Wrap>
    void
    doTestStream(Wrap const& w,
        permessage_deflate const& pmd)
    {
        using boost::asio::buffer;

        // send empty message
        doTest(pmd, [&](ws_type& ws)
        {
            ws.text(true);
            w.write(ws, boost::asio::null_buffers{});
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(b.size() == 0);
        });

        // send message
        doTest(pmd, [&](ws_type& ws)
        {
            ws.auto_fragment(false);
            ws.binary(false);
            w.write(ws, sbuf("Hello"));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(to_string(b.data()) == "Hello");
        });

        // read_some
        doTest(pmd, [&](ws_type& ws)
        {
            w.write(ws, sbuf("Hello"));
            char buf[10];
            auto const bytes_written =
                w.read_some(ws, buffer(buf, sizeof(buf)));
            BEAST_EXPECT(bytes_written > 0);
            buf[bytes_written] = 0;
            BEAST_EXPECT(
                string_view(buf).substr(0, bytes_written) ==
                string_view("Hello").substr(0, bytes_written));
        });

        // close, no payload
        doTest(pmd, [&](ws_type& ws)
        {
            w.close(ws, {});
        });

        // close with code
        doTest(pmd, [&](ws_type& ws)
        {
            w.close(ws, close_code::going_away);
        });

        // send ping and message
        doTest(pmd, [&](ws_type& ws)
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
            w.ping(ws, "");
            ws.binary(true);
            w.write(ws, sbuf("Hello"));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(once);
            BEAST_EXPECT(ws.got_binary());
            BEAST_EXPECT(to_string(b.data()) == "Hello");
        });

        // send ping and fragmented message
        doTest(pmd, [&](ws_type& ws)
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
            w.write_some(ws, false, sbuf("Hello, "));
            w.write_some(ws, false, sbuf(""));
            w.write_some(ws, true, sbuf("World!"));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(once);
            BEAST_EXPECT(to_string(b.data()) == "Hello, World!");
            ws.control_callback();
        });

        // send pong
        doTest(pmd, [&](ws_type& ws)
        {
            w.pong(ws, "");
        });

        // send auto fragmented message
        doTest(pmd, [&](ws_type& ws)
        {
            ws.auto_fragment(true);
            ws.write_buffer_size(8);
            w.write(ws, sbuf("Now is the time for all good men"));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == "Now is the time for all good men");
        });

        // send message with write buffer limit
        doTest(pmd, [&](ws_type& ws)
        {
            std::string s(2000, '*');
            ws.write_buffer_size(1200);
            w.write(ws, buffer(s.data(), s.size()));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // unexpected cont
        doTest(pmd, [&](ws_type& ws)
        {
            w.write_raw(ws, cbuf(
                0x80, 0x80, 0xff, 0xff, 0xff, 0xff));
            doCloseTest(w, ws, close_code::protocol_error);
        });

        // invalid fixed frame header
        doTest(pmd, [&](ws_type& ws)
        {
            w.write_raw(ws, cbuf(
                0x8f, 0x80, 0xff, 0xff, 0xff, 0xff));
            doCloseTest(w, ws, close_code::protocol_error);
        });

        if(! pmd.client_enable)
        {
            // expected cont
            doTest(pmd, [&](ws_type& ws)
            {
                w.write_some(ws, false, boost::asio::null_buffers{});
                w.write_raw(ws, cbuf(
                    0x81, 0x80, 0xff, 0xff, 0xff, 0xff));
                doCloseTest(w, ws, close_code::protocol_error);
            });

            // message size above 2^64
            doTest(pmd, [&](ws_type& ws)
            {
                w.write_some(ws, false, cbuf(0x00));
                w.write_raw(ws, cbuf(
                    0x80, 0xff, 0xff, 0xff, 0xff, 0xff,
                    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff));
                doCloseTest(w, ws, close_code::too_big);
            });

            /*
            // message size exceeds max
            doTest(pmd, [&](ws_type& ws)
            {
                // VFALCO This was never implemented correctly
                ws.read_message_max(1);
                w.write(ws, cbuf(0x81, 0x02, '*', '*'));
                doCloseTest(w, ws, close_code::too_big);
            });
            */
        }

        // receive ping
        doTest(pmd, [&](ws_type& ws)
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
            w.write(ws, sbuf("Hello"));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(invoked);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(to_string(b.data()) == "Hello");
        });

        // receive ping
        doTest(pmd, [&](ws_type& ws)
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
            w.write(ws, sbuf("Hello"));
            doCloseTest(w, ws, close_code::none);
        });

        // receive bad utf8
        doTest(pmd, [&](ws_type& ws)
        {
            put(ws.next_layer().buffer(), cbuf(
                0x81, 0x06, 0x03, 0xea, 0xf0, 0x28, 0x8c, 0xbc));
            doFailTest(w, ws, error::failed);
        });

        // receive bad close
        doTest(pmd, [&](ws_type& ws)
        {
            put(ws.next_layer().buffer(), cbuf(
                0x88, 0x02, 0x03, 0xed));
            doFailTest(w, ws, error::failed);
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
        testWrite();

        testOptions();
        testPausation1();
        testPausation2();
        testPausation3();
        testIssue300();
        testAsyncWriteFrame();

        auto const testStream =
            [this](permessage_deflate const& pmd)
            {
                doTestStream(SyncClient{}, pmd);

                yield_to(
                    [&](yield_context yield)
                    {
                        doTestStream(AsyncClient{yield}, pmd);
                    });
            };

        permessage_deflate pmd;

        pmd.client_enable = false;
        pmd.server_enable = false;
        testStream(pmd);

        pmd.client_enable = true;
        pmd.server_enable = true;
        pmd.client_max_window_bits = 10;
        pmd.client_no_context_takeover = false;
        pmd.compLevel = 1;
        pmd.memLevel = 1;
        testStream(pmd);

        pmd.client_enable = true;
        pmd.server_enable = true;
        pmd.client_max_window_bits = 10;
        pmd.client_no_context_takeover = true;
        pmd.compLevel = 1;
        pmd.memLevel = 1;
        testStream(pmd);
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,stream);

} // websocket
} // beast
} // boost
