//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/zlib/deflate_stream.hpp>

#include <boost/beast/core/string.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <array>
#include <cstdint>
#include <numeric>
#include <random>
#include <string>

#include "zlib-1.3.2/zlib.h"

#include "fixtures/CVE_2018_25032/default.hpp"
#include "fixtures/CVE_2018_25032/fixed.hpp"

namespace boost {
namespace beast {
namespace zlib {

class deflate_stream_test : public beast::unit_test::suite
{
    struct ICompressor {
        virtual void init() = 0;
        virtual void init(
            int level,
            int windowBits,
            int memLevel,
            int strategy) = 0;
        virtual void reset() = 0;

        virtual std::size_t avail_in() const noexcept = 0;
        virtual void avail_in(std::size_t) noexcept = 0;
        virtual void const* next_in() const noexcept = 0;
        virtual void next_in(const void*) noexcept = 0;
        virtual std::size_t avail_out() const noexcept = 0;
        virtual void avail_out(std::size_t) noexcept = 0;
        virtual void* next_out() const noexcept = 0;
        virtual void next_out(void*) noexcept = 0;
        virtual std::size_t total_out() const noexcept = 0;

        virtual std::size_t bound(std::size_t) = 0;
        virtual error_code write(Flush) = 0;
        virtual error_code params(int level, int strategy) = 0;
        virtual error_code prime(int bits, int value) = 0;
        virtual void tune(int good_length, int max_lazy, int nice_length, int max_chain) = 0;
        virtual void pending(unsigned& value, int& bits) = 0;
        virtual ~ICompressor() = default;
    };
    class ZlibCompressor : public ICompressor {
        z_stream zs{};

    public:
        ZlibCompressor() = default;
        void init() override {
            deflateEnd(&zs);
            zs = {};
            const auto res = deflateInit2(&zs, -1, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
            if(res != Z_OK)
               throw std::invalid_argument{"zlib compressor: failure"};
        }
        void init(
            int level,
            int windowBits,
            int memLevel,
            int strategy) override
        {
            deflateEnd(&zs);
            zs = {};
            const auto res = deflateInit2(&zs, level, Z_DEFLATED, -windowBits, memLevel, strategy);
            if(res != Z_OK)
               BOOST_THROW_EXCEPTION(std::invalid_argument{"zlib compressor: bad arg"});
        }
        void reset() override {
            if(deflateReset(&zs) != Z_OK)
                throw std::runtime_error{"zlib compressor: reset failed"};
        }

        virtual std::size_t avail_in() const noexcept override  { return zs.avail_in; }
        virtual void avail_in(std::size_t n) noexcept override { zs.avail_in = static_cast<uInt>(n); }
        virtual void const* next_in() const noexcept override { return zs.next_in; }
        virtual void next_in(const void* ptr) noexcept override { zs.next_in = const_cast<Bytef*>(static_cast<const Bytef*>(ptr)); }
        virtual std::size_t avail_out() const noexcept override { return zs.avail_out; }
        virtual void avail_out(std::size_t n_out) noexcept override { zs.avail_out = static_cast<uInt>(n_out); }
        virtual void* next_out() const noexcept override { return zs.next_out; }
        virtual void next_out(void* ptr) noexcept override { zs.next_out = (Bytef*)ptr; }
        virtual std::size_t total_out() const noexcept override { return zs.total_out; }

        std::size_t bound(std::size_t src_size) override {
           return deflateBound(&zs, static_cast<uLong>(src_size));
        }
        error_code write(Flush flush) override {
            constexpr static int zlib_flushes[] = {0, Z_BLOCK, Z_PARTIAL_FLUSH, Z_SYNC_FLUSH, Z_FULL_FLUSH, Z_FINISH, Z_TREES};
            const auto zlib_flush = zlib_flushes[static_cast<int>(flush)];
            const auto res = deflate(&zs, zlib_flush);
            switch(res){
            case Z_OK:
                return {};
            case Z_STREAM_END:
                return error::end_of_stream;
            case Z_STREAM_ERROR:
                return error::stream_error;
            case Z_BUF_ERROR:
                return error::need_buffers;
            default:
                throw;
            }
        }
        error_code params(int level, int strategy) override {
            switch(deflateParams(&zs, level, strategy)){
            case Z_OK:
                return {};
            case Z_STREAM_ERROR:
                return error::stream_error;
            case Z_BUF_ERROR:
                return error::need_buffers;
            default:
                throw std::runtime_error{"zlib compressor: impossible value"};
            }
        }
        error_code prime(int bits, int value) override {
            switch(deflatePrime(&zs, bits, value)){
            case Z_OK:
                return {};
            case Z_STREAM_ERROR:
                return error::stream_error;
            case Z_BUF_ERROR:
                return error::need_buffers;
            default:
                throw std::runtime_error{"zlib compressor: impossible value"};
            }
        }
        void tune(int good_length, int max_lazy, int nice_length, int max_chain) override {
            if(deflateTune(&zs, good_length, max_lazy, nice_length, max_chain) != Z_OK)
                throw std::runtime_error{"zlib compressor: tune failed"};
        }
        void pending(unsigned& value, int& bits) override {
            if(deflatePending(&zs, &value, &bits) != Z_OK)
                throw std::runtime_error{"zlib compressor: pending failed"};
        }

        ~ZlibCompressor() override {
            deflateEnd(&zs);
        }
    } zlib_compressor;
    class BeastCompressor : public ICompressor {
        z_params zp;
        deflate_stream ds;

    public:
        BeastCompressor() = default;
        void init() override {
          zp = {};
          ds.clear();
          ds.reset(
              compression::default_size,
              15,
              8,
              Strategy::normal);
        }
        void init(
            int level,
            int windowBits,
            int memLevel,
            int strategy) override
        {
            zp = {};
            ds.clear();
            ds.reset(
                level,
                windowBits,
                memLevel,
                toStrategy(strategy));
        }
        void reset() override {
            ds.reset();
        }

        virtual std::size_t avail_in() const noexcept override  { return zp.avail_in; }
        virtual void avail_in(std::size_t n) noexcept override { zp.avail_in = n; }
        virtual void const* next_in() const noexcept override { return zp.next_in; }
        virtual void next_in(const void* ptr) noexcept override { zp.next_in = ptr; }
        virtual std::size_t avail_out() const noexcept override { return zp.avail_out; }
        virtual void avail_out(std::size_t n_out) noexcept override { zp.avail_out = n_out; }
        virtual void* next_out() const noexcept override { return zp.next_out; }
        virtual void next_out(void* ptr) noexcept override { zp.next_out = (Bytef*)ptr; }
        virtual std::size_t total_out() const noexcept override { return zp.total_out; }

        std::size_t bound(std::size_t src_size) override {
            return ds.upper_bound(src_size);
        }
        error_code write(Flush flush) override {
          error_code ec{};
          ds.write(zp, flush, ec);
          return ec;
        }
        error_code params(int level, int strategy) override {
          error_code ec{};
          ds.params(zp, level, toStrategy(strategy), ec);
          return ec;
        }
        error_code prime(int bits, int value) override {
          error_code ec{};
          ds.prime(bits, value, ec);
          return ec;
        }
        void tune(int good_length, int max_lazy, int nice_length, int max_chain) override {
            ds.tune(good_length, max_lazy, nice_length, max_chain);
        }
        void pending(unsigned& value, int& bits) override {
            ds.pending(&value, &bits);
        }

        ~BeastCompressor() override = default;
    } beast_compressor;

public:
    // Lots of repeats, limited char range
    static
    std::string
    corpus1(std::size_t n)
    {
        static std::string const alphabet{
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
        };
        std::string s;
        s.reserve(n + 5);
        std::mt19937 g;
        std::uniform_int_distribution<std::size_t> d0{
            0, alphabet.size() - 1};
        std::uniform_int_distribution<std::size_t> d1{
            1, 5};
        while(s.size() < n)
        {
            auto const rep = d1(g);
            auto const ch = alphabet[d0(g)];
            s.insert(s.end(), rep, ch);
        }
        s.resize(n);
        return s;
    }

    // Random data
    static
    std::string
    corpus2(std::size_t n)
    {
        std::string s;
        s.reserve(n);
        std::mt19937 g;
        std::uniform_int_distribution<std::uint32_t> d0{0, 255};
        while(n--)
            s.push_back(static_cast<char>(d0(g)));
        return s;
    }

    static
    std::string
    compress(
        string_view const& in,
        int level,                  // 0=none, 1..9, -1=default
        int windowBits,             // 9..15
        int memLevel)               // 1..9 (8=default)
    {
        int const strategy = Z_DEFAULT_STRATEGY;
        int result;
        z_stream zs;
        memset(&zs, 0, sizeof(zs));
        result = deflateInit2(
            &zs,
            level,
            Z_DEFLATED,
            -windowBits,
            memLevel,
            strategy);
        if(result != Z_OK)
            throw std::logic_error{"deflateInit2 failed"};
        zs.next_in = (Bytef*)in.data();
        zs.avail_in = static_cast<uInt>(in.size());
        std::string out;
        out.resize(deflateBound(&zs,
            static_cast<uLong>(in.size())));
        zs.next_in = (Bytef*)in.data();
        zs.avail_in = static_cast<uInt>(in.size());
        zs.next_out = (Bytef*)&out[0];
        zs.avail_out = static_cast<uInt>(out.size());
        result = deflate(&zs, Z_FULL_FLUSH);
        if(result != Z_OK)
            throw std::logic_error("deflate failed");
        out.resize(zs.total_out);
        deflateEnd(&zs);
        return out;
    }

    static
    std::string
    decompress(string_view const& in)
    {
        int result;
        std::string out;
        z_stream zs;
        memset(&zs, 0, sizeof(zs));
        result = inflateInit2(&zs, -15);
        if(result != Z_OK)
            throw std::logic_error{"inflateInit2 failed"};
        try
        {
            zs.next_in = (Bytef*)in.data();
            zs.avail_in = static_cast<uInt>(in.size());
            for(;;)
            {
                out.resize(zs.total_out + 1024);
                zs.next_out = (Bytef*)&out[zs.total_out];
                zs.avail_out = static_cast<uInt>(
                    out.size() - zs.total_out);
                result = inflate(&zs, Z_SYNC_FLUSH);
                if( result == Z_NEED_DICT ||
                    result == Z_DATA_ERROR ||
                    result == Z_MEM_ERROR)
                {
                    throw std::logic_error("inflate failed");
                }
                if(zs.avail_out > 0)
                    break;
                if(result == Z_STREAM_END)
                    break;
            }
            out.resize(zs.total_out);
            inflateEnd(&zs);
        }
        catch(...)
        {
            inflateEnd(&zs);
            throw;
        }
        return out;
    }

    //--------------------------------------------------------------------------

    using self = deflate_stream_test;
    typedef void(self::*pmf_t)(
        ICompressor& c,
        int level, int windowBits, int memLevel,
        int strategy, std::string const&);

    static
    Strategy
    toStrategy(int strategy)
    {
        switch(strategy)
        {
        default:
        case 0: return Strategy::normal;
        case 1: return Strategy::filtered;
        case 2: return Strategy::huffman;
        case 3: return Strategy::rle;
        case 4: return Strategy::fixed;
        }
    }

    void
    doDeflate1_beast(
        ICompressor& c,
        int level, int windowBits, int memLevel,
        int strategy, std::string const& check)
    {
        std::string out;
        c.init(
            level,
            windowBits,
            memLevel,
            strategy);
        out.resize(c.bound(check.size()));
        c.next_in(check.data());
        c.avail_in(check.size());
        c.next_out((void*)out.data());
        c.avail_out(out.size());
        {
            bool progress = true;
            for(;;)
            {
                error_code ec = c.write(Flush::full);
                if( ec == error::need_buffers ||
                    ec == error::end_of_stream) // per zlib FAQ
                    goto fin;
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    goto err;
                if(! BEAST_EXPECT(progress))
                    goto err;
                progress = false;
            }
        }

    fin:
        out.resize(c.total_out());
        BEAST_EXPECT(decompress(out) == check);

    err:
        ;
    }

    //--------------------------------------------------------------------------

    void
    doDeflate2_beast(
        ICompressor& c,
        int level, int windowBits, int memLevel,
        int strategy, std::string const& check)
    {
        for(std::size_t i = 1; i < check.size(); ++i)
        {
            for(std::size_t j = 1;; ++j)
            {
                c.init(
                    level,
                    windowBits,
                    memLevel,
                    strategy);
                std::string out;
                out.resize(c.bound(check.size()));
                if(j >= out.size())
                    break;
                c.next_in((void*)check.data());
                c.avail_in(i);
                c.next_out((void*)out.data());
                c.avail_out(j);
                bool bi = false;
                bool bo = false;
                for(;;)
                {
                    error_code ec = c.write(
                        bi ? Flush::full : Flush::none);
                    if( ec == error::need_buffers ||
                        ec == error::end_of_stream) // per zlib FAQ
                        goto fin;
                    if(! BEAST_EXPECTS(! ec, ec.message()))
                        goto err;
                    if(c.avail_in() == 0 && ! bi)
                    {
                        bi = true;
                        c.avail_in(check.size() - i);
                    }
                    if(c.avail_out() == 0 && ! bo)
                    {
                        bo = true;
                        c.avail_out(out.size() - j);
                    }
                }

            fin:
                out.resize(c.total_out());
                BEAST_EXPECT(decompress(out) == check);

            err:
                ;
            }
        }
    }

    //--------------------------------------------------------------------------

    void
    doMatrix(ICompressor& c, std::string const& check, pmf_t pmf)
    {
        for(int level = 0; level <= 9; ++level)
        {
            // windowBits 8 is rejected for raw deflate,
            // see testInvalidSettings
            int const windowBits = 9;
            for(int strategy = 0; strategy <= 4; ++strategy)
            {
                for (int memLevel = 8; memLevel <= 9; ++memLevel)
                {
                    (this->*pmf)(
                        c, level, windowBits, memLevel, strategy, check);
                }
            }
        }

        // Check default settings
        (this->*pmf)(c, compression::default_size, 15, 8, 0, check);
    }

    void
    testDeflate(ICompressor& c)
    {
        doMatrix(c, "Hello, world!", &self::doDeflate1_beast);
        doMatrix(c, "Hello, world!", &self::doDeflate2_beast);
        log << "no-silence keepalive" << std::endl;
        doMatrix(c, corpus1(56), &self::doDeflate2_beast);
        doMatrix(c, corpus1(1024), &self::doDeflate1_beast);
    }

    void testInvalidSettings(ICompressor& c)
    {
        except<std::invalid_argument>(
            [&]()
            {
                c.init(-42, 15, 8, static_cast<int>(Strategy::normal));
            });
        except<std::invalid_argument>(
            [&]()
            {
                c.init(compression::default_size, -1, 8, static_cast<int>(Strategy::normal));
            });
        except<std::invalid_argument>(
            [&]()
            {
                c.init(compression::default_size, 15, -1, static_cast<int>(Strategy::normal));
            });
        except<std::invalid_argument>(
            [&]()
            {
                // a 256-byte window is not supported for raw deflate
                c.init(compression::default_size, 8, 8, static_cast<int>(Strategy::normal));
            });
    }

    void
    testNullPointers(ICompressor& c)
    {
        std::string out(64, '\0');
        string_view s = "Hello";

        c.init();
        c.next_in(s.data());
        c.avail_in(s.size());
        c.next_out(nullptr);
        c.avail_out(out.size());
        BEAST_EXPECT(c.write(Flush::full) == error::stream_error);

        c.init();
        c.next_in(nullptr);
        c.avail_in(s.size());
        c.next_out(&out[0]);
        c.avail_out(out.size());
        BEAST_EXPECT(c.write(Flush::full) == error::stream_error);

        // a null input pointer is fine when there is no input
        c.init();
        c.next_in(nullptr);
        c.avail_in(0);
        c.next_out(&out[0]);
        c.avail_out(out.size());
        BEAST_EXPECT(c.write(Flush::finish) == error::end_of_stream);
    }

    void
    testErrorCodeCleared()
    {
        // a stale error in `ec` is cleared when the operation succeeds
        std::string out(64, '\0');
        string_view s = "Hello";
        deflate_stream ds;
        z_params zp;
        zp.next_in = s.data();
        zp.avail_in = s.size();
        zp.next_out = &out[0];
        zp.avail_out = out.size();

        error_code ec = error::stream_error;
        ds.params(zp, 6, Strategy::normal, ec);
        BEAST_EXPECT(! ec);

        ec = error::stream_error;
        ds.prime(3, 5, ec);
        BEAST_EXPECT(! ec);

        ec = error::stream_error;
        ds.write(zp, Flush::none, ec);
        BEAST_EXPECT(! ec);
        BEAST_EXPECT(zp.avail_in == 0);
    }

    void
    testFlushTrees(ICompressor& c)
    {
        // Flush::trees is only meaningful for inflate
        c.init();
        std::string out;
        out.resize(64);
        string_view s = "Hello";
        c.next_in(s.data());
        c.avail_in(s.size());
        c.next_out(&out.front());
        c.avail_out(out.size());
        BEAST_EXPECT(c.write(Flush::trees) == error::stream_error);
    }

    void
    testWriteAfterFinish(ICompressor& c)
    {
        c.init();
        std::string out;
        out.resize(1024);
        string_view s = "Hello";
        c.next_in(s.data());
        c.avail_in(s.size());
        c.next_out(&out.front());
        c.avail_out(out.size());
        error_code ec = c.write(Flush::sync);
        BEAST_EXPECT(!ec);
        c.next_in(nullptr);
        c.avail_in(0);
        ec = c.write(Flush::finish);
        BEAST_EXPECT(ec == error::end_of_stream);
        c.next_in(s.data());
        c.avail_in(s.size());
        c.next_out(&out.front());
        c.avail_out(out.size());
        ec = c.write(Flush::sync);
        BEAST_EXPECT(ec == error::stream_error);
        ec = c.write(Flush::finish);
        BEAST_EXPECT(ec == error::need_buffers);
    }

    void
    testFlushPartial(ICompressor& c)
    {
        c.init();
        std::string out;
        out.resize(1024);
        string_view s = "Hello";
        c.next_in(s.data());
        c.avail_in(s.size());
        c.next_out(&out.front());
        c.avail_out(out.size());
        error_code ec;
        ec = c.write(Flush::none);
        BEAST_EXPECT(!ec);
        ec = c.write(Flush::partial);
        BEAST_EXPECT(!ec);
    }

    void
    testFlushAtLiteralBufferFull(ICompressor& c)
    {
        struct fixture
        {
            ICompressor& c;
            explicit fixture(ICompressor&c, std::size_t n, Strategy s) : c(c)
            {
                c.init(8, 15, 1, static_cast<int>(s));
                std::iota(in.begin(), in.end(), std::uint8_t{0});
                out.resize(n);
                c.next_in(in.data());
                c.avail_in(in.size());
                c.next_out(&out.front());
                c.avail_out(out.size());
            }

            std::array<std::uint8_t, 255> in;
            std::string out;
        };

        for (auto s : {Strategy::huffman, Strategy::rle, Strategy::normal})
        {
            {
                fixture f{c, 264, s};
                error_code ec = c.write(Flush::finish);
                BEAST_EXPECT(ec == error::end_of_stream);
                BEAST_EXPECT(c.avail_out() == 1);
            }

            {
                fixture f{c,263, s};
                error_code ec = c.write(Flush::finish);
                BEAST_EXPECT(!ec);
                BEAST_EXPECT(c.avail_out() == 0);
            }

            {
                fixture f{c, 20, s};
                error_code ec = c.write(Flush::sync);
                BEAST_EXPECT(!ec);
            }

        }
    }

    void
    testRLEMatchLengthExceedLookahead(ICompressor& c)
    {
        std::vector<std::uint8_t> in;
        in.resize(300);

        c.init(8, 15, 1, static_cast<int>(Strategy::rle));
        std::fill_n(in.begin(), 4, 'a');
        std::string out;
        out.resize(in.size() * 2);
        c.next_in(in.data());
        c.avail_in(in.size());
        c.next_out(&out.front());
        c.avail_out(out.size());

        error_code ec;
        ec = c.write(Flush::sync);
        BEAST_EXPECT(!ec);
    }

    void
    testFlushAfterDistMatch(ICompressor& c)
    {
        for (auto out_size : {144, 129})
        {
            std::array<std::uint8_t, 256> in{};
            // 125 will mostly fill the lit buffer, so emitting a distance code
            // results in a flush.
            auto constexpr n = 125;
            std::iota(in.begin(), in.begin() + n,
                static_cast<std::uint8_t>(0));
            std::iota(in.begin() + n, in.end(),
                static_cast<std::uint8_t>(0));

            c.init(8, 15, 1, static_cast<int>(Strategy::normal));
            std::string out;
            out.resize(out_size);
            c.next_in(in.data());
            c.avail_in(in.size());
            c.next_out(&out.front());
            c.avail_out(out.size());

            error_code ec;
            ec = c.write(Flush::sync);
            BEAST_EXPECT(!ec);
        }
    }

    void
    testCVE(char const* in, int l, Strategy s)
    {
        deflate_stream ds;
        ds.reset(l, 15, 1, s);
        z_params p;
        p.next_in = in;
        p.avail_in = std::strlen(in);
        std::size_t n = deflate_upper_bound(p.avail_in);
        std::vector<unsigned char> out(n);
        p.next_out = out.data();
        p.avail_out = n;
        error_code ec;
        BEAST_NO_THROW(ds.write(p, Flush::finish, ec));
        BEAST_EXPECT(ec == zlib::error::end_of_stream);
    }

    void
    testCVE()
    {
        testCVE(CVE_2018_25032_default, 1, Strategy::fixed);
        testCVE(CVE_2018_25032_default, 2, Strategy::fixed);
        testCVE(CVE_2018_25032_default, 6, Strategy::fixed);
        testCVE(CVE_2018_25032_fixed, 1, Strategy::normal);
        testCVE(CVE_2018_25032_fixed, 2, Strategy::normal);
        testCVE(CVE_2018_25032_fixed, 6, Strategy::normal);
    }

    //--------------------------------------------------------------------------
    //
    // Parity with the reference zlib
    //
    //--------------------------------------------------------------------------

    /*  The output of a compressor is recorded byte for byte, along with
        the result of every call, so that the two engines can be compared.
        The engines must agree on every result code as well, or else the
        sequence of calls made by the driver would not have been the same.
    */
    struct trace
    {
        std::string out;
        std::string log;
        bool finished = false;

        bool
        operator==(trace const& rhs) const
        {
            return out == rhs.out &&
                log == rhs.log &&
                finished == rhs.finished;
        }
    };

    // Record the result of one call, with the bytes it consumed and produced
    static
    void
    record(trace& t, error_code const& ec,
        std::size_t consumed, std::size_t produced)
    {
        t.log += std::to_string(ec.value());
        t.log += ':';
        t.log += std::to_string(consumed);
        t.log += ':';
        t.log += std::to_string(produced);
        t.log += ';';
    }

    static
    std::string
    describe(
        int level, int windowBits, int memLevel, int strategy,
        std::size_t size, std::size_t in_step, std::size_t out_step,
        Flush flush)
    {
        std::string s;
        s += "level=" + std::to_string(level);
        s += " windowBits=" + std::to_string(windowBits);
        s += " memLevel=" + std::to_string(memLevel);
        s += " strategy=" + std::to_string(strategy);
        s += " size=" + std::to_string(size);
        s += " in_step=" + std::to_string(in_step);
        s += " out_step=" + std::to_string(out_step);
        s += " flush=" + std::to_string(static_cast<int>(flush));
        return s;
    }

    /*  Compress `in`, feeding `in_step` bytes per round and offering
        `out_step` bytes of output space per call. Each round but the
        last uses `flush`, and the last round uses Flush::finish. A
        round calls write until the flush is complete, that is, until
        write returns with room left in the output buffer.
    */
    static
    trace
    drive(
        ICompressor& c,
        string_view in,
        std::size_t in_step,
        std::size_t out_step,
        Flush flush)
    {
        trace t;
        std::string out(in.size() * 10 + 1024, '\0');
        std::size_t ip = 0;
        std::size_t op = 0;
        for(;;)
        {
            std::size_t left = (std::min)(in_step, in.size() - ip);
            bool const last = ip + left == in.size();
            Flush const f = last ? Flush::finish : flush;
            c.next_in(in.data() + ip);
            c.avail_in(left);
            for(;;)
            {
                std::size_t const m = (std::min)(out_step, out.size() - op);
                if(m == 0)
                {
                    t.log += "overflow;";
                    t.out = out.substr(0, op);
                    return t;
                }
                c.next_out(&out[op]);
                c.avail_out(m);
                error_code const ec = c.write(f);
                std::size_t const produced = m - c.avail_out();
                std::size_t const consumed = left - c.avail_in();
                op += produced;
                ip += consumed;
                left = c.avail_in();
                record(t, ec, consumed, produced);
                if(ec == error::end_of_stream)
                {
                    t.finished = true;
                    t.out = out.substr(0, op);
                    return t;
                }
                if(ec == error::need_buffers)
                    break; // no progress is possible with this flush
                if(ec)
                {
                    t.out = out.substr(0, op);
                    return t;
                }
                if(c.avail_out() != 0)
                    break; // the flush is complete
            }
            if(last)
                break;
        }
        t.out = out.substr(0, op);
        return t;
    }

    void
    checkParity(
        int level, int windowBits, int memLevel, int strategy,
        string_view in, std::size_t in_step, std::size_t out_step,
        Flush flush)
    {
        zlib_compressor.init(level, windowBits, memLevel, strategy);
        auto const zt = drive(zlib_compressor, in, in_step, out_step, flush);
        beast_compressor.init(level, windowBits, memLevel, strategy);
        auto const bt = drive(beast_compressor, in, in_step, out_step, flush);
        auto const what = describe(level, windowBits, memLevel, strategy,
            in.size(), in_step, out_step, flush);
        BEAST_EXPECTS(zt.finished, "zlib: " + what);
        BEAST_EXPECTS(bt == zt, "beast: " + what);
    }

    void
    testParity()
    {
        std::string const small = "Hello, world!";
        std::string const text = corpus1(1024);
        std::string const random = corpus2(1024);
        Flush const flushes[] = {
            Flush::none, Flush::block, Flush::partial, Flush::sync, Flush::full};

        for(int level = 0; level <= 9; ++level)
        for(int windowBits : {9, 15})
        for(int memLevel : {1, 8})
        for(int strategy = 0; strategy <= 4; ++strategy)
        {
            // everything in one call
            for(auto const* in : {&small, &text, &random})
                checkParity(level, windowBits, memLevel, strategy,
                    *in, in->size(), in->size() * 10 + 1024, Flush::finish);

            for(auto flush : flushes)
            {
                /*  Repeating a partial, sync, or full flush into an
                    output buffer of six bytes or less emits a flush
                    marker on every call, so those flushes need a
                    slightly larger output buffer to make progress.
                */
                std::size_t const tiny =
                    flush == Flush::none || flush == Flush::block ? 1 : 7;

                // one byte in, tiny output buffer
                checkParity(level, windowBits, memLevel, strategy,
                    small, 1, tiny, flush);
                // small pieces both ways
                checkParity(level, windowBits, memLevel, strategy,
                    text, 7, tiny + 4, flush);
                checkParity(level, windowBits, memLevel, strategy,
                    random, 7, tiny + 4, flush);
                // everything in, tiny output buffer
                checkParity(level, windowBits, memLevel, strategy,
                    text, 1024, tiny + 2, flush);
                // small pieces in, big output buffer
                checkParity(level, windowBits, memLevel, strategy,
                    random, 64, 4096, flush);
            }
        }
    }

    void
    testStoredParity()
    {
        // Larger than a stored block and than the largest window, to
        // exercise block splitting and window sliding at level 0.
        auto const big = corpus1(70000);
        Flush const flushes[] = {
            Flush::none, Flush::block, Flush::partial, Flush::sync, Flush::full};
        for(int windowBits : {9, 12, 15})
        for(int memLevel : {1, 2, 8, 9})
        {
            checkParity(0, windowBits, memLevel, 0,
                big, big.size(), big.size() + 1024, Flush::finish);
            checkParity(0, windowBits, memLevel, 0,
                big, big.size(), 7, Flush::finish);
            for(auto flush : flushes)
            {
                checkParity(0, windowBits, memLevel, 0,
                    big, 4096, 100, flush);
                checkParity(0, windowBits, memLevel, 0,
                    big, 100, 4096, flush);
                checkParity(0, windowBits, memLevel, 0,
                    big, 65535, 65535, flush);
            }
        }

        // window sliding at the other levels
        for(int level : {1, 6, 9})
        for(int windowBits : {9, 15})
        for(int memLevel : {1, 8})
        for(int strategy : {0, 2, 3, 4})
        {
            checkParity(level, windowBits, memLevel, strategy,
                big, big.size(), big.size() + 1024, Flush::finish);
            checkParity(level, windowBits, memLevel, strategy,
                big, 4096, 4096, Flush::sync);
            checkParity(level, windowBits, memLevel, strategy,
                big, 5000, 1000, Flush::none);
        }
    }

    /*  Compress `in` in three pieces, changing the level and strategy
        in between the way an application would. If `params_first` is
        set the first change is made before any input is compressed.
    */
    static
    trace
    drive_params(
        ICompressor& c,
        string_view in,
        std::size_t out_step,
        int level1, int strategy1,
        int level2, int strategy2,
        bool params_first)
    {
        trace t;
        std::string out(in.size() * 10 + 1024, '\0');
        std::size_t op = 0;

        // Call write until the flush is complete
        auto const write =
            [&](string_view piece, Flush flush)
            {
                std::size_t left = piece.size();
                c.next_in(piece.data());
                c.avail_in(left);
                for(;;)
                {
                    std::size_t const m = (std::min)(out_step, out.size() - op);
                    if(m == 0)
                    {
                        t.log += "overflow;";
                        return false;
                    }
                    c.next_out(&out[op]);
                    c.avail_out(m);
                    error_code const ec = c.write(flush);
                    std::size_t const produced = m - c.avail_out();
                    std::size_t const consumed = left - c.avail_in();
                    op += produced;
                    left = c.avail_in();
                    record(t, ec, consumed, produced);
                    if(ec == error::end_of_stream)
                    {
                        t.finished = true;
                        return false;
                    }
                    if(ec == error::need_buffers)
                        return true;
                    if(ec)
                        return false;
                    if(c.avail_out() != 0)
                        return true;
                }
            };

        // Call params, giving it output space until it succeeds
        auto const params =
            [&](int level, int strategy)
            {
                c.next_in(nullptr);
                c.avail_in(0);
                for(int i = 0; i < 1000; ++i)
                {
                    std::size_t const m = (std::min)(out_step, out.size() - op);
                    if(m == 0)
                    {
                        t.log += "overflow;";
                        return false;
                    }
                    c.next_out(&out[op]);
                    c.avail_out(m);
                    error_code const ec = c.params(level, strategy);
                    std::size_t const produced = m - c.avail_out();
                    op += produced;
                    record(t, ec, 0, produced);
                    if(ec != error::need_buffers)
                        return ! ec;
                }
                t.log += "stuck;";
                return false;
            };

        auto const n = in.size() / 3;
        bool ok = true;
        if(params_first)
            ok = params(level1, strategy1) &&
                write(in.substr(0, 2 * n), Flush::none);
        else
            ok = write(in.substr(0, n), Flush::none) &&
                params(level1, strategy1) &&
                write(in.substr(n, n), Flush::none);
        if(ok && params(level2, strategy2))
            write(in.substr(2 * n), Flush::finish);
        t.out = out.substr(0, op);
        return t;
    }

    void
    checkParamsParity(
        int const* script,
        int windowBits, int memLevel,
        string_view in, std::size_t out_step,
        bool params_first)
    {
        zlib_compressor.init(script[0], windowBits, memLevel, script[1]);
        auto const zt = drive_params(zlib_compressor, in, out_step,
            script[2], script[3], script[4], script[5], params_first);
        beast_compressor.init(script[0], windowBits, memLevel, script[1]);
        auto const bt = drive_params(beast_compressor, in, out_step,
            script[2], script[3], script[4], script[5], params_first);
        std::string what = "params";
        for(int i = 0; i < 6; i += 2)
            what += " " + std::to_string(script[i]) +
                "/" + std::to_string(script[i + 1]);
        what += params_first ? " first " : " ";
        what += describe(script[0], windowBits, memLevel, script[1],
            in.size(), in.size() / 3, out_step, Flush::none);
        BEAST_EXPECTS(zt.finished, "zlib: " + what);
        BEAST_EXPECTS(bt == zt, "beast: " + what);
    }

    void
    testParamsParity()
    {
        auto const text = corpus1(3000);
        auto const random = corpus2(3000);
        auto const big = corpus1(70000);

        // level and strategy at the start, after the first
        // piece, and after the second piece
        int const scripts[][6] = {
            {0, 0,  6, 0,  0, 0}, // store, compress, store
            {6, 0,  0, 0,  9, 0}, // compress, store, compress
            {0, 0,  0, 0,  1, 0}, // store, store, fast
            {1, 0,  9, 0,  1, 0}, // fast, slow, fast
            {6, 0,  6, 2,  6, 3}, // normal, huffman, rle
            {6, 4,  6, 1,  6, 0}, // fixed, filtered, normal
            {0, 0,  6, 4,  0, 2}, // store, fixed, store
            {9, 0,  0, 0,  0, 0}, // slow, store, store
        };
        for(auto const& script : scripts)
        for(bool params_first : {false, true})
        {
            for(int windowBits : {9, 15})
            for(int memLevel : {1, 8})
            for(auto const* in : {&text, &random})
            for(std::size_t out_step : {
                std::size_t(5), std::size_t(4096), std::size_t(1 << 20)})
            {
                checkParamsParity(script, windowBits, memLevel,
                    *in, out_step, params_first);
            }

            // more than a window of input is stored before switching
            checkParamsParity(script, 15, 8, big, 1 << 20, params_first);
            checkParamsParity(script, 9, 1, big, 4096, params_first);
        }
    }

    void
    testPrimeParity()
    {
        auto const text = corpus1(500);
        for(int bits : {1, 5, 8, 13, 16})
        for(int value : {0, 1, 0x5555, 0xffff})
        for(int level : {0, 1, 6})
        {
            zlib_compressor.init(level, 15, 8, 0);
            auto const zec = zlib_compressor.prime(bits, value);
            auto const zt = drive(zlib_compressor,
                text, text.size(), 1 << 20, Flush::finish);
            beast_compressor.init(level, 15, 8, 0);
            auto const bec = beast_compressor.prime(bits, value);
            auto const bt = drive(beast_compressor,
                text, text.size(), 1 << 20, Flush::finish);
            BEAST_EXPECT(zec == bec);
            BEAST_EXPECT(zt.finished);
            BEAST_EXPECT(bt == zt);
        }

        // out of range bit counts
        for(int bits : {-1, 17, 100})
        {
            zlib_compressor.init(6, 15, 8, 0);
            beast_compressor.init(6, 15, 8, 0);
            BEAST_EXPECT(zlib_compressor.prime(bits, 0) == error::need_buffers);
            BEAST_EXPECT(beast_compressor.prime(bits, 0) == error::need_buffers);
        }
    }

    struct tuning
    {
        int good_length;
        int max_lazy;
        int nice_length;
        int max_chain;
    };

    /*  Compress `in` repeatedly with the tuning `t` applied at different
        points: before the first write, on a running stream, before a
        reset or a change of level (which discard it), and after them
        (which keep it). The traces are joined so that one comparison
        covers them all.
    */
    static
    trace
    driveTuned(
        ICompressor& c,
        int level,
        tuning const& t,
        string_view in)
    {
        trace all;
        auto const append = [&all](trace const& tr)
        {
            all.log += tr.log;
            all.log += '|';
            all.out += tr.out;
            all.finished = tr.finished;
        };
        auto const tune = [&c, &t]()
        {
            c.tune(t.good_length, t.max_lazy, t.nice_length, t.max_chain);
        };
        auto const params = [&all, &c](int level)
        {
            all.log += std::to_string(c.params(level, 0).value());
            all.log += ';';
        };

        // before the first write
        c.init(level, 15, 8, 0);
        tune();
        append(drive(c, in, 1000, 4096, Flush::sync));

        // on a running stream
        {
            c.init(level, 15, 8, 0);
            std::size_t const n = in.size() / 2;
            std::string out(in.size() + 1024, '\0');
            c.next_in(in.data());
            c.avail_in(n);
            c.next_out(&out[0]);
            c.avail_out(out.size());
            error_code const ec = c.write(Flush::sync);
            trace head;
            record(head, ec, n - c.avail_in(), out.size() - c.avail_out());
            head.out = out.substr(0, out.size() - c.avail_out());
            append(head);
            tune();
            append(drive(c, in.substr(n), 1000, 4096, Flush::sync));
        }

        // a reset restores the parameters of the level
        tune();
        c.reset();
        append(drive(c, in, 1000, 4096, Flush::sync));

        // so does a change of level, but not the same level again
        c.reset();
        tune();
        params(level == 1 ? 6 : 1);
        append(drive(c, in, 1000, 4096, Flush::sync));
        c.reset();
        tune();
        params(level);
        append(drive(c, in, 1000, 4096, Flush::sync));

        // a tuning made after a reset is kept
        c.reset();
        tune();
        append(drive(c, in, 1000, 4096, Flush::sync));

        return all;
    }

    void
    testTuneParity()
    {
        auto const text = corpus1(20000);
        tuning const tunings[] = {
            {4, 4, 8, 4},           // the level 1 configuration
            {8, 16, 128, 128},      // the level 6 configuration
            {32, 258, 258, 4096},   // the level 9 configuration
            {34, 78, 88, 26},
            {258, 258, 258, 1}
        };
        int changed = 0;
        for(int level : {1, 4, 6, 9})
        {
            zlib_compressor.init(level, 15, 8, 0);
            auto const plain = drive(
                zlib_compressor, text, 1000, 4096, Flush::sync);
            for(auto const& t : tunings)
            {
                auto const zt = driveTuned(zlib_compressor, level, t, text);
                auto const bt = driveTuned(beast_compressor, level, t, text);
                BEAST_EXPECT(zt.finished);
                BEAST_EXPECTS(bt == zt,
                    "level=" + std::to_string(level) +
                    " good_length=" + std::to_string(t.good_length) +
                    " max_lazy=" + std::to_string(t.max_lazy) +
                    " nice_length=" + std::to_string(t.nice_length) +
                    " max_chain=" + std::to_string(t.max_chain));
                // The tuning must show in the output of the first
                // scenario, or the comparison proves nothing.
                if(zt.out.compare(0, plain.out.size(), plain.out) != 0)
                    ++changed;
            }
        }
        BEAST_EXPECT(changed > 0);
    }

    /*  Record what pending reports at each step of a short session:
        on a fresh stream, after prime, with output held back by a
        full buffer, after it drains, after a partial flush (which
        ends mid-byte), after finish, and after a reset.
    */
    static
    std::string
    drivePending(ICompressor& c, string_view in)
    {
        std::string log;
        auto const note = [&c, &log]()
        {
            unsigned value = 1;
            int bits = 1;
            c.pending(value, bits);
            log += std::to_string(value);
            log += ':';
            log += std::to_string(bits);
            log += ';';
        };
        std::size_t const n = in.size() / 2;
        std::string out(in.size() + 1024, '\0');

        c.init(6, 15, 8, 0);
        note();

        c.prime(5, 0x15);
        note();

        c.next_in(in.data());
        c.avail_in(n);
        c.next_out(&out[0]);
        c.avail_out(8);
        c.write(Flush::sync);
        note();

        c.next_out(&out[0]);
        c.avail_out(out.size());
        c.write(Flush::sync);
        note();

        c.next_in(in.data() + n);
        c.avail_in(in.size() - n);
        c.next_out(&out[0]);
        c.avail_out(out.size());
        c.write(Flush::partial);
        note();

        c.next_out(&out[0]);
        c.avail_out(out.size());
        c.write(Flush::finish);
        note();

        c.reset();
        note();

        return log;
    }

    void
    testPendingParity()
    {
        auto const text = corpus1(3000);
        auto const zl = drivePending(zlib_compressor, text);
        auto const bl = drivePending(beast_compressor, text);
        BEAST_EXPECTS(bl == zl, bl + " != " + zl);
        // nothing is pending before the first write
        BEAST_EXPECT(zl.compare(0, 4, "0:0;") == 0);
    }

    void
    testBoundParity()
    {
        std::size_t const sizes[] = {
            0, 1, 13, 127, 128, 255, 256, 1023, 1024, 16383, 16384,
            32767, 32768, 65535, 65536, 1000000, std::size_t(1) << 31};
        for(int level = 0; level <= 9; ++level)
        for(int windowBits = 9; windowBits <= 15; ++windowBits)
        for(int memLevel = 1; memLevel <= 9; ++memLevel)
        {
            zlib_compressor.init(level, windowBits, memLevel, 0);
            beast_compressor.init(level, windowBits, memLevel, 0);
            for(auto n : sizes)
            {
                BEAST_EXPECTS(
                    zlib_compressor.bound(n) == beast_compressor.bound(n),
                    describe(level, windowBits, memLevel, 0,
                        n, 0, 0, Flush::none));
            }
        }

        // without a stream
        for(auto n : sizes)
            BEAST_EXPECT(deflate_upper_bound(n) ==
                deflateBound(nullptr, static_cast<uLong>(n)));
    }

    void
    testDefaultParity()
    {
        // A default constructed stream compresses like zlib
        // does with level 6, windowBits 15, and memLevel 8.
        auto const text = corpus1(3000);
        zlib_compressor.init();
        auto const zt = drive(zlib_compressor, text, 100, 4096, Flush::sync);
        BeastCompressor fresh;
        auto const bt = drive(fresh, text, 100, 4096, Flush::sync);
        BEAST_EXPECT(zt.finished);
        BEAST_EXPECT(bt == zt);
    }

    void
    run() override
    {
        testDeflate(zlib_compressor);
        testDeflate(beast_compressor);
        testInvalidSettings(zlib_compressor);
        testInvalidSettings(beast_compressor);
        testNullPointers(zlib_compressor);
        testNullPointers(beast_compressor);
        testErrorCodeCleared();
        testWriteAfterFinish(zlib_compressor);
        testWriteAfterFinish(beast_compressor);
        testFlushPartial(zlib_compressor);
        testFlushPartial(beast_compressor);
        testFlushAtLiteralBufferFull(zlib_compressor);
        testFlushAtLiteralBufferFull(beast_compressor);
        testRLEMatchLengthExceedLookahead(zlib_compressor);
        testRLEMatchLengthExceedLookahead(beast_compressor);
        testFlushAfterDistMatch(zlib_compressor);
        testFlushAfterDistMatch(beast_compressor);
        testFlushTrees(zlib_compressor);
        testFlushTrees(beast_compressor);
        testCVE();
        testParity();
        testStoredParity();
        testParamsParity();
        testPrimeParity();
        testTuneParity();
        testPendingParity();
        testBoundParity();
        testDefaultParity();
    }
};

BEAST_DEFINE_TESTSUITE(beast,zlib,deflate_stream);

} // zlib
} // beast
} // boost
