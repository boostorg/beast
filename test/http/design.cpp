//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <beast/core.hpp>
#include <beast/http.hpp>
#include <beast/core/detail/clamp.hpp>
#include <beast/core/detail/read_size_helper.hpp>
#include <beast/test/pipe_stream.hpp>
#include <beast/test/string_istream.hpp>
#include <beast/test/string_ostream.hpp>
#include <beast/test/yield_to.hpp>
#include <beast/unit_test/suite.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <sstream>

namespace beast {
namespace http {

class design_test
    : public beast::unit_test::suite
    , public beast::test::enable_yield_to
{
public:
    // two threads, for some examples using a pipe
    design_test()
        : enable_yield_to(2)
    {
    }

    template<bool isRequest>
    bool
    equal_body(string_view sv, string_view body)
    {
        test::string_istream si{
            get_io_service(), sv.to_string()};
        message<isRequest, string_body, fields> m;
        multi_buffer b;
        try
        {
            read(si, b, m);
            return m.body == body;
        }
        catch(std::exception const& e)
        {
            log << "equal_body: " << e.what() << std::endl;
            return false;
        }
    }

    //--------------------------------------------------------------------------
    //
    // Example: Expect 100-continue
    //
    //--------------------------------------------------------------------------

    /** Send a request with Expect: 100-continue

        This function will send a request with the Expect: 100-continue
        field by first sending the header, then waiting for a successful
        response from the server before continuing to send the body. If
        a non-successful server response is received, the function
        returns immediately.

        @param stream The remote HTTP server stream.

        @param buffer The buffer used for reading.

        @param req The request to send. This function modifies the object:
        the Expect header field is inserted into the message if it does
        not already exist, and set to "100-continue".

        @param ec Set to the error, if any occurred.
    */
    template<
        class SyncStream,
        class DynamicBuffer,
        class Body, class Fields>
    void
    send_expect_100_continue(
        SyncStream& stream,
        DynamicBuffer& buffer,
        request<Body, Fields>& req,
        error_code& ec)
    {
        static_assert(is_sync_stream<SyncStream>::value,
            "SyncStream requirements not met");

        static_assert(is_dynamic_buffer<DynamicBuffer>::value,
            "DynamicBuffer requirements not met");

        // Insert or replace the Expect field
        req.fields.replace("Expect", "100-continue");

        // Create the serializer
        auto sr = make_serializer(req);

        // Send just the header
        write_header(stream, sr, ec);
        if(ec)
            return;

        BOOST_ASSERT(sr.is_header_done());
        BOOST_ASSERT(! sr.is_done());

        // Read the response from the server.
        // A robust client could set a timeout here.
        {
            response<string_body> res;
            read(stream, buffer, res, ec);
            if(ec)
                return;
            if(res.status != 100)
            {
                // The server indicated that it will not
                // accept the request, so skip sending the body.
                return;
            }
        }

        // Server is OK with the request, send the body
        write(stream, sr, ec);
    }

    /** Receive a request, handling Expect: 100-continue if present.

        This function will read a request from the specified stream.
        If the request contains the Expect: 100-continue field, a
        status response will be delivered.

        @param stream The remote HTTP client stream.

        @param buffer The buffer used for reading.

        @param ec Set to the error, if any occurred.
    */
    template<
        class SyncStream,
        class DynamicBuffer>
    void
    receive_expect_100_continue(
        SyncStream& stream,
        DynamicBuffer& buffer,
        error_code& ec)
    {
        static_assert(is_sync_stream<SyncStream>::value,
            "SyncStream requirements not met");

        static_assert(is_dynamic_buffer<DynamicBuffer>::value,
            "DynamicBuffer requirements not met");

        // Declare a parser for a request with a string body
        request_parser<string_body> parser;

        // Read the header
        read_header(stream, buffer, parser, ec);
        if(ec)
            return;

        // Check for the Expect field value
        if(parser.get().fields["Expect"] == "100-continue")
        {
            // send 100 response
            response<empty_body> res;
            res.version = 11;
            res.status = 100;
            res.reason("Continue");
            res.fields.insert("Server", "test");
            write(stream, res, ec);
            if(ec)
                return;
        }

        BEAST_EXPECT(! parser.is_done());
        BEAST_EXPECT(parser.get().body.empty());

        // Read the rest of the message.
        //
        // We use parser.base() to return a basic_parser&, to avoid an
        // ambiguous function error (from boost::asio::read). Another
        // solution is to qualify the call, e.g. `beast::http::read`
        //
        read(stream, buffer, parser.base(), ec);
    }

    void
    doExpect100Continue()
    {
        test::pipe p{ios_};
        yield_to(
            [&](yield_context)
            {
                error_code ec;
                flat_buffer buffer;
                receive_expect_100_continue(
                    p.server, buffer, ec);
                BEAST_EXPECTS(! ec, ec.message());
            },
            [&](yield_context)
            {
                flat_buffer buffer;
                request<string_body, fields> req;
                req.version = 11;
                req.method("POST");
                req.target("/");
                req.fields.insert("User-Agent", "test");
                req.body = "Hello, world!";
                prepare(req);

                error_code ec;
                send_expect_100_continue(
                    p.client, buffer, req, ec);
                BEAST_EXPECTS(! ec, ec.message());
            });
    }

    //--------------------------------------------------------------------------
    //
    // Example: CGI child process relay
    //
    //--------------------------------------------------------------------------

    /** Send the output of a child process as an HTTP response.

        The output of the child process comes from a @b SyncReadStream. Data
        will be sent continuously as it is produced, without the requirement
        that the entire process output is buffered before being sent. The
        response will use the chunked transfer encoding.

        @param input A stream to read the child process output from.

        @param output A stream to write the HTTP response to.

        @param ec Set to the error, if any occurred.
    */
    template<
        class SyncReadStream,
        class SyncWriteStream>
    void
    send_cgi_response(
        SyncReadStream& input,
        SyncWriteStream& output,
        error_code& ec)
    {
        static_assert(is_sync_read_stream<SyncReadStream>::value,
            "SyncReadStream requirements not met");

        static_assert(is_sync_write_stream<SyncWriteStream>::value,
            "SyncWriteStream requirements not met");

        using boost::asio::buffer_cast;
        using boost::asio::buffer_size;

        // Set up the response. We use the buffer_body type,
        // allowing serialization to use manually provided buffers.
        message<false, buffer_body, fields> res;

        res.status = 200;
        res.version = 11;
        res.fields.insert("Server", "Beast");
        res.fields.insert("Transfer-Encoding", "chunked");

        // No data yet, but we set more = true to indicate
        // that it might be coming later. Otherwise the
        // serializer::is_done would return true right after
        // sending the header.
        res.body.data = nullptr;
        res.body.more = true;

        // Create the serializer. We set the split option to
        // produce the header immediately without also trying
        // to acquire buffers from the body (which would return
        // the error http::need_buffer because we set `data`
        // to `nullptr` above).
        auto sr = make_serializer(res);

        // Send the header immediately.
        write_header(output, sr, ec);
        if(ec)
            return;

        // Alternate between reading from the child process
        // and sending all the process output until there
        // is no more output.
        do
        {
            // Read a buffer from the child process
            char buffer[2048];
            auto bytes_transferred = input.read_some(
                boost::asio::buffer(buffer, sizeof(buffer)), ec);
            if(ec == boost::asio::error::eof)
            {
                ec = {};

                // `nullptr` indicates there is no buffer
                res.body.data = nullptr;

                // `false` means no more data is coming
                res.body.more = false;
            }
            else
            {
                if(ec)
                    return;

                // Point to our buffer with the bytes that
                // we received, and indicate that there may
                // be some more data coming
                res.body.data = buffer;
                res.body.size = bytes_transferred;
                res.body.more = true;
            }
            
            // Write everything in the body buffer
            write(output, sr, ec);

            // This error is returned by body_buffer during
            // serialization when it is done sending the data
            // provided and needs another buffer.
            if(ec == error::need_buffer)
            {
                ec = {};
                continue;
            }
            if(ec)
                return;
        }
        while(! sr.is_done());
    }

    void
    doCgiResponse()
    {
        std::string const s = "Hello, world!";
        test::pipe child{ios_};
        child.server.read_size(3);
        ostream(child.server.buffer) << s;
        child.client.close();
        test::pipe p{ios_};
        error_code ec;
        send_cgi_response(child.server, p.client, ec);
        BEAST_EXPECTS(! ec, ec.message());
        BEAST_EXPECT(equal_body<false>(p.server.str(), s));
    }

    //--------------------------------------------------------------------------
    //
    // Example: HTTP Relay
    //
    //--------------------------------------------------------------------------

    /** Relay an HTTP message.

        This function efficiently relays an HTTP message from a downstream
        client to an upstream server, or from an upstream server to a
        downstream client. After the message header is read from the input,
        a user provided transformation function is invoked which may change
        the contents of the header before forwarding to the output. This may
        be used to adjust fields such as Server, or proxy fields.

        @param output The stream to write to.

        @param input The stream to read from.

        @param buffer The buffer to use for the input.

        @param transform The header transformation to apply. The function will
        be called with this signature:
        @code
            void transform(
                header<isRequest, Fields>&,     // The header to transform
                error_code&);                   // Set to the error, if any
        @endcode

        @param ec Set to the error if any occurred.

        @tparam isRequest `true` to relay a request.

        @tparam Fields The type of fields to use for the message.
    */
    template<
        bool isRequest,
        class Fields = fields,
        class SyncWriteStream,
        class SyncReadStream,
        class DynamicBuffer,
        class Transform>
    void
    relay(
        SyncWriteStream& output,
        SyncReadStream& input,
        DynamicBuffer& buffer,
        error_code& ec,
        Transform&& transform)
    {
        static_assert(is_sync_write_stream<SyncWriteStream>::value,
            "SyncWriteStream requirements not met");

        static_assert(is_sync_read_stream<SyncReadStream>::value,
            "SyncReadStream requirements not met");

        // A small buffer for relaying the body piece by piece
        char buf[2048];

        // Create a parser with a buffer body to read from the input.
        parser<isRequest, buffer_body, Fields> p;

        // Create a serializer from the message contained in the parser.
        serializer<isRequest, buffer_body, Fields> sr{p.get()};

        // Read just the header from the input
        read_header(input, buffer, p, ec);
        if(ec)
            return;

        // Apply the caller's header tranformation
        // base() returns a reference to the header portion of the message.
        transform(p.get().base(), ec);
        if(ec)
            return;

        // Send the transformed message to the output
        write_header(output, sr, ec);
        if(ec)
            return;

        // Loop over the input and transfer it to the output
        do
        {
            if(! p.is_done())
            {
                // Set up the body for writing into our small buffer
                p.get().body.data = buf;
                p.get().body.size = sizeof(buf);

                // Read as much as we can
                read(input, buffer, p, ec);

                // This error is returned when buffer_body uses up the buffer
                if(ec == error::need_buffer)
                    ec = {};
                if(ec)
                    return;

                // Set up the body for reading.
                // This is how much was parsed:
                p.get().body.size = sizeof(buf) - p.get().body.size;
                p.get().body.data = buf;
                p.get().body.more = ! p.is_done();
            }
            else
            {
                p.get().body.data = nullptr;
                p.get().body.size = 0;
            }

            // Write everything in the buffer (which might be empty)
            write(output, sr, ec);

            // This error is returned when buffer_body uses up the buffer
            if(ec == error::need_buffer)
                ec = {};
            if(ec)
                return;
        }
        while(! p.is_done() && ! sr.is_done());
    }

    void
    doRelay()
    {
        request<string_body, fields> req;
        req.version = 11;
        req.method("POST");
        req.target("/");
        req.fields.insert("User-Agent", "test");
        req.body = "Hello, world!";
        prepare(req);

        test::pipe downstream{ios_};
        downstream.server.read_size(3);
        test::pipe upstream{ios_};
        upstream.client.write_size(3);

        error_code ec;
        write(downstream.client, req);
        BEAST_EXPECTS(! ec, ec.message());
        downstream.client.close();

        flat_buffer buffer;
        relay<true>(upstream.client, downstream.server, buffer, ec,
            [&](header<true, fields>& h, error_code& ec)
            {
                h.fields.erase("Content-Length");
                h.fields.replace("Transfer-Encoding", "chunked");
            });
        BEAST_EXPECTS(! ec, ec.message());
        BEAST_EXPECT(equal_body<true>(
            upstream.server.str(), req.body));
    }

    //--------------------------------------------------------------------------
    //
    // Example: Parse from std::istream
    //
    //--------------------------------------------------------------------------

    /** Parse an HTTP/1 message from a `std::istream`.

        This function attempts to parse a complete message from the stream.

        @param is The `std::istream` to read from.

        @param buffer The buffer to use.

        @param msg The message to store the result.

        @param ec Set to the error, if any occurred.
    */
    template<
        class Allocator,
        bool isRequest,
        class Body,
        class Fields>
    void
    parse_istream(
        std::istream& is,
        basic_flat_buffer<Allocator>& buffer,
        message<isRequest, Body, Fields>& msg,
        error_code& ec)
    {
        // Create the message parser
        parser<isRequest, Body, Fields> parser;

        do
        {
            // Extract whatever characters are presently available in the istream
            if(is.rdbuf()->in_avail() > 0)
            {
                // Get a mutable buffer sequence for writing
                auto const mb = buffer.prepare(is.rdbuf()->in_avail());

                // Now get everything we can from the istream
                buffer.commit(is.readsome(
                    boost::asio::buffer_cast<char*>(mb),
                    boost::asio::buffer_size(mb)));
            }
            else if(buffer.size() == 0)
            {
                // Our buffer is empty and we need more characters, 
                // see if we've reached the end of file on the istream
                if(! is.eof())
                {
                    // Get a mutable buffer sequence for writing
                    auto const mb = buffer.prepare(1024);

                    // Try to get more from the istream. This might block.
                    is.read(
                        boost::asio::buffer_cast<char*>(mb),
                        boost::asio::buffer_size(mb));

                    // If an error occurs on the istream then return it to the caller.
                    if(is.fail() && ! is.eof())
                    {
                        // We'll just re-use io_error since std::istream has no error_code interface.
                        ec = make_error_code(errc::io_error);
                        return;
                    }

                    // Commit the characters we got to the buffer.
                    buffer.commit(is.gcount());
                }
                else
                {
                    // Inform the parser that we've reached the end of the istream.
                    parser.put_eof(ec);
                    if(ec)
                        return;
                    break;
                }
            }

            // Write the data to the parser
            auto const bytes_used = parser.put(buffer.data(), ec);

            // This error means that the parser needs additional octets.
            if(ec == error::need_more)
                ec = {};
            if(ec)
                return;

            // Consume the buffer octets that were actually parsed.
            buffer.consume(bytes_used);
        }
        while(! parser.is_done());

        // Transfer ownership of the message container in the parser to the caller.
        msg = parser.release();
    }

    void
    doParseStdStream()
    {
        std::istringstream is(
            "HTTP/1.0 200 OK\r\n"
            "User-Agent: test\r\n"
            "\r\n"
            "Hello, world!"
        );
        error_code ec;
        flat_buffer buffer;
        response<string_body> res;
        parse_istream(is, buffer, res, ec);
        BEAST_EXPECTS(! ec, ec.message());
    }

    //--------------------------------------------------------------------------
    //
    // Deferred Body type commitment
    //
    //--------------------------------------------------------------------------

    void
    doDeferredBody()
    {
        test::pipe p{ios_};
        ostream(p.server.buffer) <<
            "POST / HTTP/1.1\r\n"
            "User-Agent: test\r\n"
            "Content-Length: 13\r\n"
            "\r\n"
            "Hello, world!";

        flat_buffer buffer;
        header_parser<true, fields> parser;
        auto bytes_used =
            read_some(p.server, buffer, parser);
        buffer.consume(bytes_used);

        request_parser<string_body> parser2(
            std::move(parser));

        while(! parser2.is_done())
        {
            bytes_used = read_some(p.server, buffer, parser2);
            buffer.consume(bytes_used);
        }
    }

    //--------------------------------------------------------------------------
#if 0
    // VFALCO This is broken
    /*
        Efficiently relay a message from one stream to another
    */
    template<
        bool isRequest,
        class SyncWriteStream,
        class DynamicBuffer,
        class SyncReadStream>
    void
    relay(
        SyncWriteStream& out,
        DynamicBuffer& b,
        SyncReadStream& in)
    {
        flat_buffer buffer{4096}; // 4K limit
        header_parser<isRequest, fields> parser;
        serializer<isRequest, buffer_body<
            typename flat_buffer::const_buffers_type>,
                fields> ws{parser.get()};
        error_code ec;
        do
        {
      	    auto const state0 = parser.state();
            auto const bytes_used =
                read_some(in, buffer, parser, ec);
            BEAST_EXPECTS(! ec, ec.message());
            switch(state0)
            {
            case parse_state::header:
            {
                BEAST_EXPECT(parser.is_header_done());
                for(;;)
                {
                    ws.write_some(out, ec);
                    if(ec == http::error::need_more)
                    {
                        ec = {};
                        break;
                    }
                    if(ec)
                        BOOST_THROW_EXCEPTION(system_error{ec});
                }
                break;
            }

            case parse_state::chunk_header:
            {
                // inspect parser.chunk_extension() here
                if(parser.is_done())
                    boost::asio::write(out,
                        chunk_encode_final());
                break;
            }

            case parse_state::body:
            case parse_state::body_to_eof:
            case parse_state::chunk_body:
            {
                if(! parser.is_done())
                {
                    auto const body = parser.body();
                    boost::asio::write(out, chunk_encode(
                        false, boost::asio::buffer(
                            body.data(), body.size())));
                }
                break;
            }
          
            case parse_state::complete:
                break;
            }
            buffer.consume(bytes_used);
        }
        while(! parser.is_done());
    }

    void
    testRelay()
    {
        // Content-Length
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "Content-Length: 5\r\n"
                "\r\n" // 37 byte header
                "*****",
                3 // max_read
            };
            test::string_ostream os{ios_};
            flat_buffer b{16};
            relay<true>(os, b, is);
        }

        // end of file
        {
            test::string_istream is{ios_,
                "HTTP/1.1 200 OK\r\n"
                "\r\n" // 19 byte header
                "*****",
                3 // max_read
            };
            test::string_ostream os{ios_};
            flat_buffer b{16};
            relay<false>(os, b, is);
        }

        // chunked
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                "5;x;y=1;z=\"-\"\r\n*****\r\n"
                "3\r\n---\r\n"
                "1\r\n+\r\n"
                "0\r\n\r\n",
                2 // max_read
            };
            test::string_ostream os{ios_};
            flat_buffer b{16};
            relay<true>(os, b, is);
        }
    }
#endif
    
    //--------------------------------------------------------------------------

    void
    run()
    {
        doExpect100Continue();
        doCgiResponse();
        doRelay();
        doParseStdStream();
        doDeferredBody();
    }
};

BEAST_DEFINE_TESTSUITE(design,http,beast);

} // http
} // beast
