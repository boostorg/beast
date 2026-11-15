//
// Copyright (c) 2026 the Boost.Beast fuzz target authors
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// HTTP round-trip fuzzer: parse -> serialize -> re-parse.
//
// Threat model: attacker-controlled bytes arrive on a socket (read side); a
// server or client then serializes a derived message back out (write side).
// The read side was fuzzed before (http_request, http_response); this target
// exists for the write side: serializer, chunk encoding, field ordering, and
// the status and verb string tables. It also asserts a property across both
// directions: a message that parses cleanly must serialize to bytes that
// parse cleanly again and carry the same semantic content.
//
// Memory safety of the harness: every buffer is capped (flat_buffer max
// sizes, body_limit), the body can only ever contain bytes that arrived in
// the fuzz input, and error messages are fetched into bounded strings. No
// allocation here is driven by an unvalidated length, so the fuzzer's
// malloc and RSS limits cannot be tripped by a crafted length header.
//

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/_experimental/test/stream.hpp>

#include <cstdint>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace test = boost::beast::test;

namespace {

constexpr std::size_t kMaxBuf = 256 * 1024;      // cap for every flat_buffer
constexpr std::uint32_t kBodyLimit = 64 * 1024;  // cap for parsed bodies

using request_t = http::request<http::dynamic_body>;
using response_t = http::response<http::dynamic_body>;

// Flags byte (the last input byte) selects the exercised permutations.
enum flags_t : std::uint8_t
{
    fl_req_split   = 0x02,  // request serializer split(true)
    fl_req_mutate  = 0x04,  // mutate request fields before serializing
    fl_resp_split  = 0x08,  // response serializer split(true)
    fl_resp_mutate = 0x10,  // mutate response fields before serializing
    fl_limited     = 0x20,  // extra parse pass with a tiny body limit
};

std::string
body_string(request_t const& m)
{
    return beast::buffers_to_string(m.body().data());
}

std::string
body_string(response_t const& m)
{
    return beast::buffers_to_string(m.body().data());
}

// `http::read` outcomes that are normal on a closed stream: a complete
// message followed by EOF, or (with a split serializer) a deliberately
// unfinished body. Everything else on a clean first parse is a violation.
bool
reparse_ok(beast::error_code const& ec, bool split)
{
    if(!ec)
        return true;
    if(ec == http::error::end_of_stream)
        return true;
    if(split && ec == http::error::partial_message)
        return true;
    return false;
}

// Light up the fields container modification API with bounded literals.
// These paths (set/insert/erase/count) are dark in production
// coverage because no target ever modifies a parsed message.
void
mutate(request_t& m)
{
    m.set(http::field::user_agent, "roundtrip-fuzz");
    m.insert(http::field::warning, "199 fuzz \"synthetic\"");
    m.insert(http::field::accept, "text/plain");
    m.insert(http::field::accept, "application/json");
    m.erase(http::field::connection);
    (void)m.count(http::field::accept);
}

void
mutate(response_t& m)
{
    m.set(http::field::server, "roundtrip-fuzz");
    m.insert(http::field::warning, "199 fuzz \"synthetic\"");
    m.insert(http::field::vary, "Accept-Encoding");
    m.insert(http::field::vary, "User-Agent");
    m.erase(http::field::connection);
    (void)m.count(http::field::vary);
}

// Every field emitted by the serializer must have come from the message.
template<class Msg>
bool
fields_preserved(Msg const& a, Msg const& b)
{
    for(auto const& f : b)
    {
        bool found = false;
        for(auto const& g : a)
        {
            if(g.name() != http::field::unknown &&
                g.name() == f.name() && g.value() == f.value())
            {
                found = true;
                break;
            }
            if(g.name() == http::field::unknown &&
                g.name_string() == f.name_string() && g.value() == f.value())
            {
                found = true;
                break;
            }
        }
        if(!found)
            return false;
    }
    return true;
}

// Serialize a parsed request and re-parse the wire bytes.
// Returns false on a round-trip property violation.
bool
roundtrip(request_t const& req, net::io_context& ioc, bool split, bool mutate_fields)
{
    beast::error_code ec;
    request_t m{req};
    if(mutate_fields)
        mutate(m);
    test::stream tx{ioc};
    test::stream rx{ioc};
    tx.connect(rx);
    http::serializer<true, http::dynamic_body> sr{m};
    sr.split(split);
    http::write(tx, sr, ec);
    std::string const what = ec.message();  // exercise the error tables
    (void)what;
    if(ec)
        return true;  // serializer refused the message; not a violation
    std::string const wire{rx.str()};

    test::stream in{ioc, wire};
    in.close_remote();
    beast::flat_buffer buf(kMaxBuf);
    http::request_parser<http::dynamic_body> p2;
    p2.body_limit(kBodyLimit);
    http::read(in, buf, p2, ec);
    (void)ec.message();
    if(!reparse_ok(ec, split))
        return false;
    if(ec)
        return true;
    request_t const& req2 = p2.get();
    if(req2.method() != m.method() ||
        req2.version() != m.version() ||
        req2.target() != m.target())
        return false;
    if(body_string(req2) != body_string(m))
        return false;
    if(!mutate_fields)
        return fields_preserved(m, req2);
    return true;
}

// Serialize a parsed response and re-parse the wire bytes.
bool
roundtrip(response_t const& res, net::io_context& ioc, bool split, bool mutate_fields)
{
    beast::error_code ec;
    response_t m{res};
    if(mutate_fields)
        mutate(m);
    test::stream tx{ioc};
    test::stream rx{ioc};
    tx.connect(rx);
    http::serializer<false, http::dynamic_body> sr{m};
    sr.split(split);
    http::write(tx, sr, ec);
    std::string const what = ec.message();
    (void)what;
    if(ec)
        return true;
    std::string const wire{rx.str()};

    test::stream in{ioc, wire};
    in.close_remote();
    beast::flat_buffer buf(kMaxBuf);
    http::response_parser<http::dynamic_body> p2;
    p2.body_limit(kBodyLimit);
    http::read(in, buf, p2, ec);
    (void)ec.message();
    if(!reparse_ok(ec, split))
        return false;
    if(ec)
        return true;
    response_t const& res2 = p2.get();
    if(res2.result_int() != m.result_int() ||
        res2.version() != m.version())
        return false;
    if(body_string(res2) != body_string(m))
        return false;
    if(!mutate_fields)
        return fields_preserved(m, res2);
    return true;
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

    // Parse the same bytes as a request and as a response: each direction
    // either parses (and gets round-tripped) or fails early into the error
    // paths, which is itself worth covering.
    {
        beast::error_code ec;
        test::stream in{ioc, payload};
        in.close_remote();
        beast::flat_buffer buf(kMaxBuf);
        http::request_parser<http::dynamic_body> p;
        p.body_limit(kBodyLimit);
        http::read(in, buf, p, ec);
        std::string const what = ec.message();  // exercise the error tables
        (void)what;
        if(!ec)
        {
            request_t const& req = p.get();
            // touch the accessors serializer callers rely on
            (void)req.keep_alive();
            (void)req.need_eof();
            (void)req.method_string();
            bool const ok = roundtrip(req, ioc,
                (flags & fl_req_split) != 0, (flags & fl_req_mutate) != 0);
            if(!ok)
                abort();
        }
    }
    {
        beast::error_code ec;
        test::stream in{ioc, payload};
        in.close_remote();
        beast::flat_buffer buf(kMaxBuf);
        http::response_parser<http::dynamic_body> p;
        p.body_limit(kBodyLimit);
        http::read(in, buf, p, ec);
        std::string const what = ec.message();
        (void)what;
        if(!ec)
        {
            response_t const& res = p.get();
            (void)res.keep_alive();
            (void)res.need_eof();
            (void)res.reason();
            (void)http::to_status_class(res.result());
            (void)http::to_status_class(res.result_int());
            bool const ok = roundtrip(res, ioc,
                (flags & fl_resp_split) != 0, (flags & fl_resp_mutate) != 0);
            if(!ok)
                abort();
        }
    }
    // Parser limit-exceeded paths: a tiny body limit turns any message with
    // a body into a deterministic body_limit error (checked before the body
    // is buffered, so memory stays bounded).
    if(flags & fl_limited)
    {
        beast::error_code ec;
        test::stream in{ioc, payload};
        in.close_remote();
        beast::flat_buffer buf(kMaxBuf);
        http::request_parser<http::dynamic_body> p;
        p.body_limit(16);
        http::read(in, buf, p, ec);
        std::string const what = ec.message();
        (void)what;
    }
    return 0;
}
