//
// Copyright (c) 2026 the Boost.Beast fuzz target authors
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// zlib codec round-trip fuzzer (component API).
//
// Threat model: Beast's own zlib fork runs attacker-influenced bytes in both
// directions whenever a websocket connection negotiates permessage-deflate:
// the compressor compresses payloads under attacker-negotiated parameters
// (level, window bits, memory level, strategy) with flush points chosen by
// the peer, and the decompressor inflates whatever the peer sends. No target
// exercises the codec directly; the websocket_server target only reaches a
// thin slice of one deflate configuration. This harness sweeps the parameter
// matrix and flush sequences and asserts the round-trip property: inflating
// a deflated payload must reproduce the payload exactly.
//
// Memory safety of the harness: the payload is a capped slice of the fuzz
// input, the compressed buffer is sized with slack above
// deflate_stream::upper_bound, the inflate buffer is one payload cap plus
// slack, and all parameters are clamped to documented valid ranges.
//

#include <boost/beast/zlib.hpp>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace beast = boost::beast;
namespace zlib = beast::zlib;

namespace {

constexpr std::size_t kMaxIn = 32 * 1024;    // payload cap
constexpr std::size_t kMaxComp = 96 * 1024;  // upper_bound(32K) + slack
constexpr std::size_t kMaxOut = 64 * 1024;   // payload cap + slack

// Input layout: [payload ...][param byte][plan byte][flags byte]
//   param byte: level = v % 10 (0..9), windowBits = 9 + ((v >> 4) % 7) (9..15)
//   plan byte:  memLevel = 1 + ((v >> 4) % 8) (1..8), strategy = v % 5,
//               flush plan = (v / 5) % 8
//   flags byte: bit0 params() switch, bit1 corrupt-stream inflate,
//               bit2 second stream with fresh parameters,
//               bit3 chunked inflate

struct deflate_result_t
{
    std::size_t size = 0;
    bool ok = false;
};

deflate_result_t
deflate_buffer(zlib::deflate_stream& ds,
    const char* in, std::size_t in_size,
    char* out, std::size_t out_size,
    zlib::Flush const* plan, std::size_t plan_size)
{
    zlib::z_params zp{};
    zp.next_in = in;
    zp.avail_in = in_size;
    zp.next_out = out;
    zp.avail_out = out_size;
    for(std::size_t i = 0; i <= plan_size; ++i)
    {
        zlib::Flush const flush =
            i < plan_size ? static_cast<zlib::Flush>(plan[i])
                          : zlib::Flush::finish;
        beast::error_code ec;
        ds.write(zp, flush, ec);
        std::string const m = ec.message();
        (void)m;
        if(ec &&
            ec != zlib::error::need_buffers &&
            !(flush == zlib::Flush::finish &&
              ec == zlib::error::end_of_stream))
        {
            std::fprintf(stderr,
                "beast-zlib-fuzz: deflate write %d flush=%d ec=%s\n",
                static_cast<int>(i), static_cast<int>(flush),
                ec.message().c_str());
            return {};  // finish reports end_of_stream on success;
            // need_buffers is documented as not always a failure
        }
    }
    return {zp.total_out, true};
}

// Returns true when the inflate outcome is consistent with the input.
bool
inflate_and_check(zlib::inflate_stream& is,
    const char* comp, std::size_t comp_size,
    const char* expect, std::size_t expect_size,
    bool chunked)
{
    std::string out(kMaxOut, '\0');
    zlib::z_params zp{};
    zp.next_in = comp;
    zp.avail_in = comp_size;
    zp.next_out = &out[0];
    zp.avail_out = out.size();
    beast::error_code ec;
    if(chunked)
    {
        // resume paths: feed the compressed stream through multiple writes
        std::size_t const step = comp_size / 3 + 1;
        std::size_t done = 0;
        while(done < comp_size)
        {
            std::size_t const n =
                (comp_size - done) < step ? (comp_size - done) : step;
            zp.next_in = comp + done;
            zp.avail_in = n;
            is.write(zp,
                done + n == comp_size ? zlib::Flush::finish
                                      : zlib::Flush::none,
                ec);
            std::string const m = ec.message();
            (void)m;
            if(ec == zlib::error::end_of_stream)
                break;
            if(ec)
                return false;
            done += n;
        }
        if(done < comp_size && !ec)
        {
            zp.next_in = comp + done;
            zp.avail_in = comp_size - done;
            is.write(zp, zlib::Flush::finish, ec);
            std::string const m = ec.message();
            (void)m;
        }
    }
    else
    {
        is.write(zp, zlib::Flush::finish, ec);
        std::string const m = ec.message();
        (void)m;
        if(ec && ec != zlib::error::end_of_stream)
            return false;
    }
    return zp.total_out == expect_size &&
        std::memcmp(&out[0], expect, expect_size) == 0;
}

} // namespace

extern "C"
int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if(size < 4)
        return 0;
    const std::uint8_t x = data[size - 1];
    const std::uint8_t f = data[size - 2];
    const std::uint8_t p = data[size - 3];
    const std::size_t payload_len =
        (size - 3) < kMaxIn ? (size - 3) : kMaxIn;
    const char* payload = reinterpret_cast<const char*>(data);

    int const level = p % 10;
    int const wbits = 9 + ((p >> 4) % 7);      // 9..15
    int const mem_level = 1 + ((f >> 4) % 8);  // 1..8
    int const strategy_idx = f % 5;
    zlib::Strategy const strategy =
        static_cast<zlib::Strategy>(strategy_idx);

    // flush sequences (finish is always appended as the last step).
    // Flush::trees is intentionally absent: beast::zlib's deflate_stream
    // rejects it unconditionally with error::stream_error (an explicit
    // guard in detail/deflate_stream.ipp), even though the enum documents
    // it; sweeps here stick to the accepted flush values.
    static zlib::Flush const plans[8][2] = {
        {zlib::Flush::sync,   zlib::Flush::none},
        {zlib::Flush::none,   zlib::Flush::none},  // empty plan (plan_size 0)
        {zlib::Flush::block,  zlib::Flush::full},
        {zlib::Flush::partial, zlib::Flush::sync},
        {zlib::Flush::none,   zlib::Flush::none},
        {zlib::Flush::full,   zlib::Flush::none},
        {zlib::Flush::sync,   zlib::Flush::sync},
        {zlib::Flush::block,  zlib::Flush::sync},
    };
    std::size_t const plan_idx = (f / 5) % 8;
    zlib::Flush const* plan = plans[plan_idx];
    std::size_t const plan_size = plan_idx == 1 ? 0 : 2;

    std::string comp(kMaxComp, '\0');
    beast::error_code ec;
    zlib::deflate_stream ds;
    ds.reset(level, wbits, mem_level, strategy);

    deflate_result_t r = deflate_buffer(ds, payload, payload_len,
        &comp[0], comp.size(), plan, plan_size);
    if(!r.ok)
        abort();  // valid parameters and buffers: deflate must not fail

    if(x & 0x01)
    {
        // dynamically switch level/strategy mid-stream, then finish
        int const level2 = (level + 3) % 10;
        zlib::Strategy const strat2 =
            static_cast<zlib::Strategy>((strategy_idx + 2) % 5);
        zlib::z_params zp{};
        zp.next_in = payload;
        zp.avail_in = 0;
        zp.next_out = &comp[r.size];
        zp.avail_out = comp.size() - r.size;
        ds.params(zp, level2, strat2, ec);
        std::string const m = ec.message();
        (void)m;
    }

    {
        zlib::inflate_stream is;
        is.reset(15);
        if(!inflate_and_check(is, &comp[0], r.size, payload, payload_len,
            (x & 0x08) != 0))
            abort();  // round-trip property violated
    }

    if(x & 0x02)
    {
        // corrupted compressed stream: any error is acceptable
        std::string bad = comp;
        std::size_t const flip = payload_len % r.size;
        bad[flip] = static_cast<char>(bad[flip] ^ 0xA5);
        zlib::inflate_stream is;
        is.reset(15);
        std::string out(kMaxOut, '\0');
        zlib::z_params zp{};
        zp.next_in = &bad[0];
        zp.avail_in = r.size;
        zp.next_out = &out[0];
        zp.avail_out = out.size();
        is.write(zp, zlib::Flush::finish, ec);
        std::string const m = ec.message();
        (void)m;
    }

    if(x & 0x04)
    {
        // a second stream with fresh valid parameters over the same payload
        zlib::deflate_stream ds2;
        ds2.reset((level + 7) % 10, 15, 1 + (mem_level % 8),
            static_cast<zlib::Strategy>((strategy_idx + 1) % 5));
        deflate_result_t r2 = deflate_buffer(ds2, payload, payload_len,
            &comp[0], comp.size(), plan, plan_size);
        if(!r2.ok)
            abort();
        zlib::inflate_stream is2;
        is2.reset(wbits);
        if(!inflate_and_check(is2, &comp[0], r2.size, payload, payload_len,
            false))
            abort();
    }
    return 0;
}
