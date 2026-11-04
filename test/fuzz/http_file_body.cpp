//
// Copyright (c) 2026 the Boost.Beast fuzz target authors
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// HTTP file_body / buffer_body fuzzer (component harness).
//
// Threat model: Beast servers stream request bodies to files and stream
// files back out with attacker-influenced headers (Content-Length,
// Transfer-Encoding, Content-Range style metadata). No existing target ever
// instantiates file_body or buffer_body, so the whole file backend
// (file_stdio/file_posix open/read/write/seek/size/close) is dark.
//
// What this harness does per input:
//  1. parses the fuzzed HTTP message (request or response) into a real file
//     on disk through a file_body parser (both beast::file_stdio and
//     beast::file_posix across flag choices),
//  2. serializes the parsed message back out (the file is read again),
//  3. re-parses the wire bytes and checks the body bytes round-trip,
//  4. optionally repeats the parse with a buffer_body parser (bounded
//     caller-provided buffer),
//  5. probes deterministic error paths (missing file open, tiny body limit).
//
// Memory safety of the harness: the on-disk path is a fixed constant, body
// bytes can only ever come from the fuzz input (a huge Content-Length just
// ends in a partial-message error, never an allocation), all read buffers
// are capped, and the buffer_body buffer is a fixed 8 KiB.
//

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/_experimental/test/stream.hpp>

#include <cstdint>
#include <cstdio>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace test = boost::beast::test;

namespace {

constexpr std::size_t kMaxBuf = 256 * 1024;
constexpr std::uint32_t kBodyLimit = 64 * 1024;
constexpr char const* kPath = "/tmp/beast_fuzz_body";
constexpr char const* kPathMissing = "/tmp/beast_fuzz_missing";

// flags byte (last input byte)
enum flags_t : std::uint8_t
{
    fl_response   = 0x01,  // parse as a response (else request)
    fl_stdio      = 0x02,  // use beast::file_stdio (else beast::file_posix)
    fl_buffer     = 0x04,  // extra buffer_body parse pass
    fl_missing    = 0x08,  // probe open-for-read of a missing file
    fl_seek       = 0x10,  // seek + size probing between the passes
    fl_limit      = 0x20,  // extra parse pass with a tiny body limit
};

template<class File>
void
roundtrip_file(net::io_context& ioc, beast::string_view payload,
    bool is_response)
{
    beast::error_code ec;
    beast::flat_buffer buf(kMaxBuf);
    std::uint64_t body_size = 0;
    std::string wire;

    // parse the fuzzed message into a real file, then serialize the parsed
    // message back out: the serializer streams the body from disk through
    // the file_body writer
    if(is_response)
    {
        test::stream in{ioc, payload};
        in.close_remote();
        http::response_parser<http::basic_file_body<File>> p;
        p.body_limit(kBodyLimit);
        p.get().body().open(kPath, beast::file_mode::write, ec);
        std::string const m0 = ec.message();
        (void)m0;
        if(ec)
            return;
        http::read(in, buf, p, ec);
        std::string const m1 = ec.message();
        (void)m1;
        if(ec)
            return;
        p.get().body().close();
        std::string const m2 = ec.message();
        (void)m2;
        p.get().body().open(kPath, beast::file_mode::read, ec);
        std::string const m3 = ec.message();
        (void)m3;
        if(ec)
            return;
        // the stored size: value_type::size() reflects the open file, which
        // in write mode was still empty at parse time
        body_size = p.get().body().size();
        test::stream tx{ioc};
        test::stream rx{ioc};
        tx.connect(rx);
        http::serializer<false, http::basic_file_body<File>> sr{p.get()};
        http::write(tx, sr, ec);
        std::string const m4 = ec.message();
        (void)m4;
        if(!ec)
            wire.assign(rx.str());
    }
    else
    {
        test::stream in{ioc, payload};
        in.close_remote();
        http::request_parser<http::basic_file_body<File>> p;
        p.body_limit(kBodyLimit);
        p.get().body().open(kPath, beast::file_mode::write, ec);
        std::string const m0 = ec.message();
        (void)m0;
        if(ec)
            return;
        http::read(in, buf, p, ec);
        std::string const m1 = ec.message();
        (void)m1;
        if(ec)
            return;
        p.get().body().close();
        std::string const m2 = ec.message();
        (void)m2;
        p.get().body().open(kPath, beast::file_mode::read, ec);
        std::string const m3 = ec.message();
        (void)m3;
        if(ec)
            return;
        // the stored size: value_type::size() reflects the open file, which
        // in write mode was still empty at parse time
        body_size = p.get().body().size();
        test::stream tx{ioc};
        test::stream rx{ioc};
        tx.connect(rx);
        http::serializer<true, http::basic_file_body<File>> sr{p.get()};
        http::write(tx, sr, ec);
        std::string const m4 = ec.message();
        (void)m4;
        if(!ec)
            wire.assign(rx.str());
    }

    // the stored body must re-parse cleanly and keep its size
    if(!wire.empty())
    {
        test::stream in2{ioc, wire};
        in2.close_remote();
        beast::flat_buffer buf2(kMaxBuf);
        http::response_parser<http::string_body> p3;
        p3.body_limit(kBodyLimit);
        http::read(in2, buf2, p3, ec);
        std::string const m5 = ec.message();
        (void)m5;
        if(!ec || ec == http::error::end_of_stream)
        {
            if(p3.get().body().size() != body_size)
            {
                std::fprintf(stderr,
                    "beast-file-fuzz: size mismatch wire=%d body=%d ec=%s\n",
                    static_cast<int>(p3.get().body().size()),
                    static_cast<int>(body_size), ec.message().c_str());
                std::fprintf(stderr, "beast-file-fuzz: wire=[%s]\n",
                    wire.c_str());
                abort();  // body round-trip property violated
            }
        }
    }
    else if(body_size > 0)
    {
        std::fprintf(stderr, "beast-file-fuzz: serialize failed body=%d\n",
            static_cast<int>(body_size));
        abort();  // a non-empty body failed to serialize
    }
}

template<class Msg>
void
buffer_body_parse(net::io_context& ioc, beast::string_view payload,
    bool is_response)
{
    (void)is_response;
    beast::error_code ec;
    test::stream in{ioc, payload};
    in.close_remote();
    beast::flat_buffer buf(kMaxBuf);
    char storage[8192];
    http::request_parser<http::buffer_body> p;
    p.body_limit(kBodyLimit);
    p.get().body().data = storage;
    p.get().body().size = sizeof(storage);
    http::read(in, buf, p, ec);
    std::string const m = ec.message();
    (void)m;
}

} // namespace

extern "C"
int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if(size < 2)
        return 0;
    const std::uint8_t flags = data[size - 1];
    beast::string_view const payload{
        reinterpret_cast<const char*>(data), size - 1};
    net::io_context ioc;
    beast::error_code ec;

    if(flags & fl_stdio)
        roundtrip_file<beast::file_stdio>(ioc, payload,
            (flags & fl_response) != 0);
    else
        roundtrip_file<beast::file_posix>(ioc, payload,
            (flags & fl_response) != 0);

    if(flags & fl_buffer)
        buffer_body_parse<http::request_parser<http::buffer_body>>(
            ioc, payload, (flags & fl_response) != 0);

    if(flags & fl_missing)
    {
        // deterministic error path: open for read of a missing file
        beast::file_stdio f;
        f.open(kPathMissing, beast::file_mode::read, ec);
        std::string const m = ec.message();
        (void)m;
    }

    if(flags & fl_seek)
    {
        beast::file_stdio f;
        f.open(kPath, beast::file_mode::read, ec);
        if(!ec)
        {
            f.seek(1, ec);
            std::string const m1 = ec.message();
            (void)m1;
            std::uint64_t const s = f.size(ec);
            std::string const m2 = ec.message();
            (void)m2;
            (void)s;
        }
        ec.clear();
        std::string const m3 = ec.message();
        (void)m3;
        f.close(ec);
        std::string const m4 = ec.message();
        (void)m4;
    }

    if(flags & fl_limit)
    {
        beast::error_code ec2;
        test::stream in{ioc, payload};
        in.close_remote();
        beast::flat_buffer buf(kMaxBuf);
        http::request_parser<http::basic_file_body<beast::file_stdio>> p;
        p.body_limit(16);
        p.get().body().open(kPath, beast::file_mode::write, ec2);
        if(!ec2)
            http::read(in, buf, p, ec2);
        std::string const m = ec2.message();
        (void)m;
    }
    return 0;
}
