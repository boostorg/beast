//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/zlib/inflate_stream.hpp>

#include <boost/beast/core/string.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <chrono>
#include <random>
#include <string>

#include "zlib-1.3.2/zlib.h"

namespace boost {
namespace beast {
namespace zlib {

class inflate_stream_test : public beast::unit_test::suite
{
    struct IDecompressor {
        virtual void init() = 0;
        virtual void init(int windowBits) = 0;

        virtual std::size_t avail_in() const noexcept = 0;
        virtual void avail_in(std::size_t) noexcept = 0;
        virtual void const* next_in() const noexcept = 0;
        virtual void next_in(const void*) noexcept = 0;
        virtual std::size_t avail_out() const noexcept = 0;
        virtual void avail_out(std::size_t) noexcept = 0;
        virtual void* next_out() const noexcept = 0;
        virtual void next_out(void*) noexcept = 0;

        virtual error_code write(Flush) = 0;
        virtual ~IDecompressor() = default;
    };
    class ZlibDecompressor : public IDecompressor {
       z_stream zs;

    public:
        ZlibDecompressor() = default;
        void init(int windowBits) override
        {
            inflateEnd(&zs);
            zs = {};
            // negative for a raw deflate stream
            const auto res = inflateInit2(&zs, -windowBits);
            switch(res){
            case Z_OK:
               break;
            case Z_MEM_ERROR:
               throw std::runtime_error{"zlib decompressor: no memory"};
            case Z_STREAM_ERROR:
               throw std::domain_error{"zlib decompressor: bad arg"};
            }
        }
        void init() override {
            inflateEnd(&zs);
            zs = {};
            const auto res = inflateInit2(&zs, -15);
            switch(res){
            case Z_OK:
                break;
            case Z_MEM_ERROR:
                throw std::runtime_error{"zlib decompressor: no memory"};
            case Z_STREAM_ERROR:
                throw std::domain_error{"zlib decompressor: bad arg"};
            }
        }

        virtual std::size_t avail_in() const noexcept override  { return zs.avail_in; }
        virtual void avail_in(std::size_t n) noexcept override { zs.avail_in = static_cast<uInt>(n); }
        virtual void const* next_in() const noexcept override { return zs.next_in; }
        virtual void next_in(const void* ptr) noexcept override { zs.next_in = const_cast<Bytef*>(static_cast<const Bytef*>(ptr)); }
        virtual std::size_t avail_out() const noexcept override { return zs.avail_out; }
        virtual void avail_out(std::size_t n_out) noexcept override { zs.avail_out = static_cast<uInt>(n_out); }
        virtual void* next_out() const noexcept override { return zs.next_out; }
        virtual void next_out(void* ptr) noexcept override { zs.next_out = (Bytef*)ptr; }

        error_code write(Flush flush) override {
            constexpr static int zlib_flushes[] = {0, Z_BLOCK, Z_PARTIAL_FLUSH, Z_SYNC_FLUSH, Z_FULL_FLUSH, Z_FINISH, Z_TREES};
            const auto zlib_flush = zlib_flushes[static_cast<int>(flush)];
            const auto res = inflate(&zs, zlib_flush);
            switch(res){
            case Z_OK:
              return {};
            case Z_STREAM_END:
              return error::end_of_stream;
            case Z_NEED_DICT:
              return error::need_dict;
            case Z_DATA_ERROR:
            case Z_STREAM_ERROR:
              return error::stream_error;
            case Z_MEM_ERROR:
              throw std::bad_alloc{};
            case Z_BUF_ERROR:
              return error::need_buffers;
            default:
              throw std::runtime_error{"zlib decompressor: impossible value"};
            }
        }

        ~ZlibDecompressor() override {
          inflateEnd(&zs);
        }
    } zlib_decompressor{};
    class BeastCompressor : public IDecompressor {
        z_params zp;
        inflate_stream is;

    public:
        BeastCompressor() = default;

        void init(int windowBits) override
        {
            zp = {};
            is.clear();
            is.reset(windowBits);
        }
        void init() override {
          zp = {};
          is.clear();
          is.reset();
        }

        virtual std::size_t avail_in() const noexcept override  { return zp.avail_in; }
        virtual void avail_in(std::size_t n) noexcept override { zp.avail_in = n; }
        virtual void const* next_in() const noexcept override { return zp.next_in; }
        virtual void next_in(const void* ptr) noexcept override { zp.next_in = ptr; }
        virtual std::size_t avail_out() const noexcept override { return zp.avail_out; }
        virtual void avail_out(std::size_t n_out) noexcept override { zp.avail_out = n_out; }
        virtual void* next_out() const noexcept override { return zp.next_out; }
        virtual void next_out(void* ptr) noexcept override { zp.next_out = (Bytef*)ptr; }

        error_code write(Flush flush) override {
            error_code ec{};
            is.write(zp, flush, ec);
            return ec;
        }

        ~BeastCompressor() override = default;
    } beast_decompressor{};
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
        int memLevel,               // 1..9 (8=default)
        int strategy,               // e.g. Z_DEFAULT_STRATEGY
        int flush = Z_FULL_FLUSH)   // e.g. Z_FINISH
    {
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
        result = deflate(&zs, flush);
        if(result != (flush == Z_FINISH ? Z_STREAM_END : Z_OK))
            throw std::logic_error("deflate failed");
        out.resize(zs.total_out);
        deflateEnd(&zs);
        return out;
    }

    //--------------------------------------------------------------------------

    enum Split
    {
        once,
        half,
        full
    };

    class Beast
    {
        Split in_;
        Split check_;
        Flush flush_;

    public:
        Beast(Split in, Split check, Flush flush = Flush::sync)
            : in_(in)
            , check_(check)
            , flush_(flush)
        {
        }

        void
        operator()(
            int window,
            std::string const& in,
            std::string const& check,
            unit_test::suite& suite) const
        {
            auto const f =
            [&](std::size_t i, std::size_t j)
            {
                std::string out(check.size(), 0);
                z_params zs;
                zs.next_in = in.data();
                zs.next_out = &out[0];
                zs.avail_in = i;
                zs.avail_out = j;
                inflate_stream is;
                is.reset(window);
                bool bi = ! (i < in.size());
                bool bo = ! (j < check.size());
                for(;;)
                {
                    error_code ec;
                    is.write(zs, flush_, ec);
                    if( ec == error::need_buffers ||
                        ec == error::end_of_stream)
                    {
                        out.resize(zs.total_out);
                        suite.expect(out == check, __FILE__, __LINE__);
                        break;
                    }
                    if(ec)
                    {
                        suite.fail(ec.message(), __FILE__, __LINE__);
                        break;
                    }
                    if(zs.avail_in == 0 && ! bi)
                    {
                        bi = true;
                        zs.avail_in = in.size() - i;
                    }
                    if(zs.avail_out == 0 && ! bo)
                    {
                        bo = true;
                        zs.avail_out = check.size() - j;
                    }
                }
            };

            std::size_t i0, i1;
            std::size_t j0, j1;

            switch(in_)
            {
            default:
            case once: i0 = in.size();     i1 = i0;         break;
            case half: i0 = in.size() / 2; i1 = i0;         break;
            case full: i0 = 1;             i1 = in.size();  break;
            }

            switch(check_)
            {
            default:
            case once: j0 = check.size();     j1 = j0;              break;
            case half: j0 = check.size() / 2; j1 = j0;              break;
            case full: j0 = 1;                j1 = check.size();    break;
            }

            for(std::size_t i = i0; i <= i1; ++i)
                for(std::size_t j = j0; j <= j1; ++j)
                    f(i, j);
        }
    };

    class Matrix
    {
        unit_test::suite& suite_;

        int level_[2];
        int window_[2];
        int strategy_[2];

    public:
        explicit
        Matrix(unit_test::suite& suite)
            : suite_(suite)
        {
            level_[0] = 0;
            level_[1] = 9;
            window_[0] = 9;
            window_[1] = 15;
            strategy_[0] = 0;
            strategy_[1] = 4;
        }

        void
        level(int from, int to)
        {
            level_[0] = from;
            level_[1] = to;
        }

        void
        level(int what)
        {
            level(what, what);
        }

        void
        window(int from, int to)
        {
            window_[0] = from;
            window_[1] = to;
        }

        void
        window(int what)
        {
            window(what, what);
        }

        void
        strategy(int from, int to)
        {
            strategy_[0] = from;
            strategy_[1] = to;
        }

        void
        strategy(int what)
        {
            strategy(what, what);
        }

        template<class F>
        void
        operator()(
            F const& f,
            std::string const& check) const
        {
            for(auto level = level_[0];
                level <= level_[1]; ++level)
            {
                for(auto window = window_[0];
                    window <= window_[1]; ++window)
                {
                    for(auto strategy = strategy_[0];
                        strategy <= strategy_[1]; ++strategy)
                        f(
                            window,
                            compress(check, level, window, 4, strategy),
                            check,
                            suite_);
                }
            }
        }
    };

    void
    testInflate(IDecompressor&)
    {
        {
            Matrix m{*this};
            std::string check =
                "{\n   \"AutobahnPython/0.6.0\": {\n"
                "      \"1.1.1\": {\n"
                "         \"behavior\": \"OK\",\n"
                "         \"behaviorClose\": \"OK\",\n"
                "         \"duration\": 2,\n"
                "         \"remoteCloseCode\": 1000,\n"
                "         \"reportfile\": \"autobahnpython_0_6_0_case_1_1_1.json\"\n"
                ;
            m(Beast{half, half}, check);
        }

        {
            Matrix m{*this};
            auto const check = corpus1(5000);
            m(Beast{half, half}, check);
        }
        {
            Matrix m{*this};
            auto const check = corpus2(5000);
            m(Beast{half, half}, check);
        }
        {
            Matrix m{*this};
            auto const check = corpus1(1000);
            m.level(6);
            m.window(9);
            m.strategy(Z_DEFAULT_STRATEGY);
            m(Beast{once, full}, check);
        }
        {
            Matrix m{*this};
            auto const check = corpus2(1000);
            m.level(6);
            m.window(9);
            m.strategy(Z_DEFAULT_STRATEGY);
            m(Beast{once, full}, check);
        }
        {
            Matrix m{*this};
            m.level(6);
            m.window(9);
            auto const check = corpus1(200);
            m(Beast{full, full}, check);
        }
        {
            Matrix m{*this};
            m.level(6);
            m.window(9);
            auto const check = corpus2(500);
            m(Beast{full, full}, check);
        }
        {
            Matrix m{*this};
            auto const check = corpus2(1000);
            m.level(6);
            m.window(9);
            m.strategy(Z_DEFAULT_STRATEGY);
            m(Beast{full, once, Flush::block}, check);
        }
        {
            // Check a known string - this provides more stable coverage,
            // independent of the RNG-generated strings.
            Matrix m{*this};
            auto const check =
                "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
                "eiusmod tempor incididunt ut labore et dolore magna aliqua. "
                "Ultricies mi eget mauris pharetra et ultrices neque ornare. Eget est "
                "lorem ipsum dolor. Dui faucibus in ornare quam viverra orci "
                "sagittis. Lorem mollis aliquam ut porttitor. Pretium quam vulputate "
                "dignissim suspendisse in est ante in. Tempus egestas sed sed risus "
                "pretium quam vulputate dignissim. Pellentesque dignissim enim sit "
                "amet venenatis urna. Eleifend quam adipiscing vitae proin sagittis "
                "nisl rhoncus. Aliquam etiam erat velit scelerisque in. Accumsan in "
                "nisl nisi scelerisque eu ultrices vitae auctor eu.";
            m.level(6);
            m.window(9);
            m.strategy(Z_DEFAULT_STRATEGY);
            m(Beast{full, full}, check);
        }

        // VFALCO Fails, but I'm unsure of what the correct
        //        behavior of Z_TREES/Flush::trees is.
#if 0
        {
            Matrix m{*this};
            auto const check = corpus2(10000);
            m.level(6);
            m.window(9);
            m.strategy(Z_DEFAULT_STRATEGY);
            m(Beast{full, once, Flush::trees}, check);
        }
#endif

        check({0x63, 0x18, 0x05, 0x40, 0x0c, 0x00}, {}, 8,  3);
        check({0xed, 0xc0, 0x81, 0x00, 0x00, 0x00, 0x00, 0x80,
               0xa0, 0xfd, 0xa9, 0x17, 0xa9, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x06}, {});
    }

    std::string check(
        std::initializer_list<std::uint8_t> const& in,
        error_code expected,
        std::size_t window_size = 15,
        std::size_t len = -1)
    {
        std::string out(1024, 0);
        z_params zs;
        inflate_stream is;
        is.reset(static_cast<int>(window_size));
        boost::system::error_code ec;

        zs.next_in = &*in.begin();
        zs.next_out = &out[0];
        zs.avail_in = (std::min)(in.size(), len);
        zs.avail_out = out.size();

        while (zs.avail_in > 0 && !ec)
        {
            is.write(zs, Flush::sync, ec);
            auto n = (std::min)(zs.avail_in, len);
            zs.next_in = static_cast<char const*>(zs.next_in) + n;
            zs.avail_in -= n;
        }

        BEAST_EXPECT(ec == expected);
        return out;
    }

    void testInflateErrors(IDecompressor&)
    {
        check({0x00, 0x00, 0x00, 0x00, 0x00},
            error::invalid_stored_length);
        check({0x03, 0x00},
            error::end_of_stream);
        check({0x06},
            error::invalid_block_type);
        check({0xfc, 0x00, 0x00},
            error::too_many_symbols);
        check({0x04, 0x00, 0xfe, 0xff},
            error::incomplete_length_set);
        check({0x04, 0x00, 0x24, 0x49, 0x00},
            error::invalid_bit_length_repeat);
        check({0x04, 0x00, 0x24, 0xe9, 0xff, 0xff},
            error::invalid_bit_length_repeat);
        check({0x04, 0x00, 0x24, 0xe9, 0xff, 0x6d},
            error::missing_eob);
        check({0x04, 0x80, 0x49, 0x92, 0x24, 0x49, 0x92, 0x24,
               0x71, 0xff, 0xff, 0x93, 0x11, 0x00},
            error::over_subscribed_length);
        check({0x04, 0x80, 0x49, 0x92, 0x24, 0x0f, 0xb4, 0xff,
               0xff, 0xc3, 0x84},
            error::incomplete_length_set);
        check({0x04, 0xc0, 0x81, 0x08, 0x00, 0x00, 0x00, 0x00,
               0x20, 0x7f, 0xeb, 0x0b, 0x00, 0x00},
            error::invalid_literal_length);
        check({0x02, 0x7e, 0xff, 0xff},
            error::invalid_distance_code);
        check({0x0c, 0xc0, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x90, 0xff, 0x6b, 0x04, 0x00},
            error::invalid_distance);
        check({0x05,0xe0, 0x81, 0x91, 0x24, 0xcb, 0xb2, 0x2c,
               0x49, 0xe2, 0x0f, 0x2e, 0x8b, 0x9a, 0x47, 0x56,
               0x9f, 0xfb, 0xfe, 0xec, 0xd2, 0xff, 0x1f},
            error::end_of_stream);
        check({0xed, 0xc0, 0x01, 0x01, 0x00, 0x00, 0x00, 0x40,
               0x20, 0xff, 0x57, 0x1b, 0x42, 0x2c, 0x4f},
            error::end_of_stream);
        check({0x02, 0x08, 0x20, 0x80, 0x00, 0x03, 0x00},
            error::end_of_stream);
        check({0x78, 0x9c, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01, 0x78, 0x9c, 0xff},
            error::invalid_stored_length);
    }

    void testInvalidSettings(IDecompressor& d)
    {
        except<std::domain_error>(
            [&]()
            {
                d.init(7);
            });
    }

    void testFixedHuffmanFlushTrees(IDecompressor& d)
    {
        std::string out(5, 0);
        d.init();
        boost::system::error_code ec;
        std::initializer_list<std::uint8_t> in = {
            0xf2, 0x48, 0xcd, 0xc9, 0xc9, 0x07, 0x00, 0x00,
            0x00, 0xff, 0xff};
        d.next_in(&*in.begin());
        d.next_out(&out[0]);
        d.avail_in(in.size());
        d.avail_out(out.size());
        ec = d.write(Flush::trees);
        BEAST_EXPECT(!ec);
        ec = d.write(Flush::sync);
        BEAST_EXPECT(!ec);
        BEAST_EXPECT(d.avail_out() == 0);
        BEAST_EXPECT(out == "Hello");
    }

    void testUncompressedFlushTrees(IDecompressor& d)
    {
        std::string out(5, 0);
        d.init();
        boost::system::error_code ec;
        std::initializer_list<std::uint8_t> in = {
            0x00, 0x05, 0x00, 0xfa, 0xff, 0x48, 0x65, 0x6c,
            0x6c, 0x6f, 0x00, 0x00};
        d.next_in(&*in.begin());
        d.next_out(&out[0]);
        d.avail_in(in.size());
        d.avail_out(out.size());
        ec = d.write(Flush::trees);
        BEAST_EXPECT(!ec);
        ec = d.write(Flush::sync);
        BEAST_EXPECT(!ec);
        BEAST_EXPECT(d.avail_out() == 0);
        BEAST_EXPECT(out == "Hello");
    }

    void
    testNullPointers(IDecompressor& d)
    {
        std::string out(16, '\0');
        std::initializer_list<std::uint8_t> in = {0x03, 0x00};

        d.init();
        d.next_in(&*in.begin());
        d.avail_in(in.size());
        d.next_out(nullptr);
        d.avail_out(out.size());
        BEAST_EXPECT(d.write(Flush::sync) == error::stream_error);

        d.init();
        d.next_in(nullptr);
        d.avail_in(in.size());
        d.next_out(&out[0]);
        d.avail_out(out.size());
        BEAST_EXPECT(d.write(Flush::sync) == error::stream_error);

        // a null input pointer is fine when there is no input
        d.init();
        d.next_in(nullptr);
        d.avail_in(0);
        d.next_out(&out[0]);
        d.avail_out(out.size());
        BEAST_EXPECT(d.write(Flush::sync) == error::need_buffers);
    }

    //--------------------------------------------------------------------------
    //
    // Parity with the reference zlib
    //
    //--------------------------------------------------------------------------

    /*  The output of a decompressor is recorded byte for byte, along with
        the result of every call, so that the two engines can be compared.
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

    /*  Decompress `in`, feeding `in_step` bytes per round and offering
        `out_step` bytes of output space per call, using `flush` on every
        call. A round calls write until its input is consumed and there
        is room left in the output buffer, or until no progress is
        possible. Flush::block and Flush::trees return early, so those
        rounds keep calling until no progress is possible.
    */
    static
    trace
    drive(
        IDecompressor& d,
        string_view in,
        std::size_t out_size,
        std::size_t in_step,
        std::size_t out_step,
        Flush flush)
    {
        trace t;
        std::string out(out_size + 1024, '\0');
        std::size_t ip = 0;
        std::size_t op = 0;
        for(;;)
        {
            std::size_t left = (std::min)(in_step, in.size() - ip);
            d.next_in(in.data() + ip);
            d.avail_in(left);
            for(int calls = 0;; ++calls)
            {
                std::size_t const m = (std::min)(out_step, out.size() - op);
                if(m == 0 || calls > 100000)
                {
                    t.log += m == 0 ? "overflow;" : "stuck;";
                    t.out = out.substr(0, op);
                    return t;
                }
                d.next_out(&out[op]);
                d.avail_out(m);
                error_code const ec = d.write(flush);
                std::size_t const produced = m - d.avail_out();
                std::size_t const consumed = left - d.avail_in();
                op += produced;
                ip += consumed;
                left = d.avail_in();
                record(t, ec, consumed, produced);
                if(ec == error::end_of_stream)
                {
                    t.finished = true;
                    t.out = out.substr(0, op);
                    return t;
                }
                if(ec == error::need_buffers)
                    break; // more input is needed
                if(ec)
                {
                    t.out = out.substr(0, op);
                    return t;
                }
                if(left == 0 && d.avail_out() != 0 &&
                    flush != Flush::block && flush != Flush::trees)
                    break; // the round is complete
            }
            if(ip >= in.size())
                break;
        }
        t.out = out.substr(0, op);
        return t;
    }

    void
    checkParity(
        int level, int windowBits, int strategy, int zflush,
        std::string const& check,
        std::size_t in_step, std::size_t out_step, Flush flush)
    {
        auto const in = compress(check, level, windowBits, 8, strategy, zflush);
        zlib_decompressor.init(windowBits);
        auto const zt = drive(zlib_decompressor,
            in, check.size(), in_step, out_step, flush);
        beast_decompressor.init(windowBits);
        auto const bt = drive(beast_decompressor,
            in, check.size(), in_step, out_step, flush);
        std::string what;
        what += "level=" + std::to_string(level);
        what += " windowBits=" + std::to_string(windowBits);
        what += " strategy=" + std::to_string(strategy);
        what += " zflush=" + std::to_string(zflush);
        what += " size=" + std::to_string(check.size());
        what += " in_step=" + std::to_string(in_step);
        what += " out_step=" + std::to_string(out_step);
        what += " flush=" + std::to_string(static_cast<int>(flush));
        if(zflush == Z_FINISH)
            BEAST_EXPECTS(zt.finished && zt.out == check, "zlib: " + what);
        BEAST_EXPECTS(bt == zt, "beast: " + what);
    }

    void
    testParity()
    {
        std::string const small = "Hello, world!";
        auto const text = corpus1(1024);
        auto const random = corpus2(1024);
        auto const big = corpus1(70000);
        auto const all = std::size_t(-1);
        Flush const flushes[] = {
            Flush::none, Flush::sync, Flush::block, Flush::trees};

        for(int level : {0, 1, 6, 9})
        for(int windowBits : {9, 15})
        for(int strategy : {0, 2, 3, 4})
        for(int zflush : {Z_FINISH, Z_FULL_FLUSH})
        {
            for(auto const* check : {&small, &text, &random})
            {
                // everything in one call
                checkParity(level, windowBits, strategy, zflush,
                    *check, all, all, Flush::sync);
                for(auto flush : flushes)
                {
                    // one byte in, one byte out
                    checkParity(level, windowBits, strategy, zflush,
                        *check, 1, 1, flush);
                    // small pieces both ways
                    checkParity(level, windowBits, strategy, zflush,
                        *check, 7, 5, flush);
                    // everything in, tiny output buffer
                    checkParity(level, windowBits, strategy, zflush,
                        *check, all, 3, flush);
                    // small pieces in, big output buffer
                    checkParity(level, windowBits, strategy, zflush,
                        *check, 64, 4096, flush);
                }
            }
            checkParity(level, windowBits, strategy, zflush,
                big, all, all, Flush::sync);
            checkParity(level, windowBits, strategy, zflush,
                big, 4096, 1000, Flush::none);
        }
    }

    void
    testFinalCodeInLastByte(IDecompressor& d)
    {
        /*  Five distinct 9-bit literals after the 3-bit block header fill
            exactly six bytes, so the 7-bit end-of-block code lies entirely
            within the last byte. The stream must decode completely when no
            input follows it, which requires pulling input one byte at a
            time rather than filling the bit buffer to the table width.
        */
        std::string const check = "\x90\x91\x92\x93\x94";
        auto const in = compress(check, 6, 15, 8, Z_FIXED, Z_FINISH);
        if(! BEAST_EXPECT(in.size() == 7))
            return;
        for(auto flush : {Flush::none, Flush::sync, Flush::finish})
        {
            std::string out(16, '\0');
            d.init(15);
            d.next_in(in.data());
            d.avail_in(in.size());
            d.next_out(&out[0]);
            d.avail_out(out.size());
            BEAST_EXPECT(d.write(flush) == error::end_of_stream);
            BEAST_EXPECT(d.avail_in() == 0);
            BEAST_EXPECT(out.substr(0, check.size()) == check);
        }
    }

    void
    testClear()
    {
        /*  clear() puts the stream in a newly constructed state, so a
            stream abandoned halfway through, with the window and the
            bit buffer in use, must decode another one from its first
            byte. The window size is kept.
        */
        auto const check = corpus1(3000);
        auto const in = compress(check, 6, 9, 8, Z_DEFAULT_STRATEGY, Z_FINISH);
        std::string out(check.size(), '\0');
        inflate_stream is;
        is.reset(9);
        {
            z_params zs{};
            zs.next_in = in.data();
            zs.avail_in = in.size() / 2;
            zs.next_out = &out[0];
            zs.avail_out = out.size();
            error_code ec;
            is.write(zs, Flush::sync, ec);
            BEAST_EXPECT(! ec);
            BEAST_EXPECT(zs.total_out > 0 && zs.total_out < check.size());
        }
        for(int i = 0; i < 2; ++i)
        {
            // the second round clears a completed stream
            is.clear();
            z_params zs{};
            zs.next_in = in.data();
            zs.avail_in = in.size();
            zs.next_out = &out[0];
            zs.avail_out = out.size();
            error_code ec;
            is.write(zs, Flush::finish, ec);
            BEAST_EXPECT(ec == error::end_of_stream);
            BEAST_EXPECT(zs.total_out == check.size());
            BEAST_EXPECT(out == check);
        }
        {
            // clear is harmless on a fresh stream
            inflate_stream fresh;
            fresh.clear();
            z_params zs{};
            zs.next_in = in.data();
            zs.avail_in = in.size();
            zs.next_out = &out[0];
            zs.avail_out = out.size();
            error_code ec;
            fresh.write(zs, Flush::finish, ec);
            BEAST_EXPECT(ec == error::end_of_stream);
            BEAST_EXPECT(out == check);
        }
    }

    void
    testFastWindowWrap()
    {
        /*  A match decoded by the fast path whose source lies across the
            physical end of the sliding window and then continues into the
            output of the current call. The stream is one fixed Huffman
            block: 514 literals with 8-bit codes, a match of 258 bytes at
            distance 3, eight more literals, end of block. With a 512-byte
            window written in two calls of 300 and 214 bytes, the write
            index is 2 after the literals: the two newest bytes sit at the
            start of the window. The match then takes one byte from the end
            of the window, those two bytes, and the rest from what it has
            just produced.
        */
        struct bits
        {
            std::string out;
            unsigned buf = 0;
            unsigned n = 0;

            // extra bits and header fields go least significant bit first
            void put(unsigned value, unsigned len)
            {
                buf |= value << n;
                n += len;
                while(n >= 8)
                {
                    out.push_back(static_cast<char>(buf & 0xff));
                    buf >>= 8;
                    n -= 8;
                }
            }

            // Huffman codes go most significant bit first
            void code(unsigned c, unsigned len)
            {
                unsigned r = 0;
                for(unsigned i = 0; i < len; ++i)
                    r = (r << 1) | ((c >> i) & 1);
                put(r, len);
            }

            void finish()
            {
                if(n)
                    put(0, 8 - n);
            }
        };
        bits b;
        std::string check;
        b.put(1, 1); // final block
        b.put(1, 2); // fixed Huffman codes
        for(unsigned i = 0; i < 514; ++i)
        {
            // literals 0..143 have 8-bit codes 0x30..0xbf
            auto const v = (i * 7) % 144;
            check.push_back(static_cast<char>(v));
            b.code(0x30 + v, 8);
        }
        b.code(0xc5, 8); // length code 285: 258 bytes
        b.code(2, 5);    // distance code 2: distance 3
        for(unsigned i = 0; i < 258; ++i)
            check.push_back(check[check.size() - 3]);
        for(unsigned i = 0; i < 8; ++i)
        {
            auto const v = 100 + i;
            check.push_back(static_cast<char>(v));
            b.code(0x30 + v, 8);
        }
        b.code(0, 7); // end of block
        b.finish();
        auto const& in = b.out;
        // 3 + 514*8 + 8 + 5 + 8*8 + 7 bits: the match code starts
        // three bits into byte 514 and ends in byte 515
        if(! BEAST_EXPECT(in.size() == 525))
            return;

        // the reference decoder accepts the stream
        {
            std::string zout(check.size(), '\0');
            zlib_decompressor.init(9);
            zlib_decompressor.next_in(in.data());
            zlib_decompressor.avail_in(in.size());
            zlib_decompressor.next_out(&zout[0]);
            zlib_decompressor.avail_out(zout.size());
            BEAST_EXPECT(zlib_decompressor.write(Flush::finish) ==
                error::end_of_stream);
            BEAST_EXPECT(zout == check);
        }

        inflate_stream is;
        is.reset(9);
        std::string out(check.size(), '\0');
        z_params zs{};
        error_code ec;

        // 300 literals, limited by the output space
        zs.next_in = in.data();
        zs.avail_in = 515;
        zs.next_out = &out[0];
        zs.avail_out = 300;
        is.write(zs, Flush::none, ec);
        BEAST_EXPECT(! ec);
        BEAST_EXPECT(zs.avail_out == 0);

        // the other 214 literals, limited by the input: the five
        // available bits of the match code cannot be decoded yet
        zs.next_out = &out[300];
        zs.avail_out = out.size() - 300;
        is.write(zs, Flush::none, ec);
        BEAST_EXPECT(! ec);
        BEAST_EXPECT(zs.avail_in == 0);
        BEAST_EXPECT(zs.total_out == 514);

        // the match is the first code the fast path decodes
        zs.next_in = in.data() + 515;
        zs.avail_in = in.size() - 515;
        is.write(zs, Flush::finish, ec);
        BEAST_EXPECT(ec == error::end_of_stream);
        BEAST_EXPECT(zs.total_out == check.size());
        BEAST_EXPECT(out == check);
    }

    void
    run() override
    {
        testInflate(zlib_decompressor);
        testInflate(beast_decompressor);
        testInflateErrors(zlib_decompressor);
        testInflateErrors(beast_decompressor);
        testInvalidSettings(zlib_decompressor);
        testInvalidSettings(beast_decompressor);
        testFixedHuffmanFlushTrees(zlib_decompressor);
        testFixedHuffmanFlushTrees(beast_decompressor);
        testUncompressedFlushTrees(zlib_decompressor);
        testUncompressedFlushTrees(beast_decompressor);
        testNullPointers(zlib_decompressor);
        testNullPointers(beast_decompressor);
        testFinalCodeInLastByte(zlib_decompressor);
        testFinalCodeInLastByte(beast_decompressor);
        testClear();
        testFastWindowWrap();
        testParity();
    }
};

BEAST_DEFINE_TESTSUITE(beast,zlib,inflate_stream);

} // zlib
} // beast
} // boost
