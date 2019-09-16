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
#include <cstdint>
#include <random>

#include "zlib-1.2.11/zlib.h"

namespace boost {
namespace beast {
namespace zlib {

class deflate_stream_test : public beast::unit_test::suite
{
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
        int level, int windowBits, int memLevel,
        int strategy, std::string const& check)
    {
        std::string out;
        z_params zs;
        deflate_stream ds;
        ds.reset(
            level,
            windowBits,
            memLevel,
            toStrategy(strategy));
        out.resize(ds.upper_bound(
            static_cast<uLong>(check.size())));
        zs.next_in = (Bytef*)check.data();
        zs.avail_in = static_cast<uInt>(check.size());
        zs.next_out = (Bytef*)out.data();
        zs.avail_out = static_cast<uInt>(out.size());
        {
            bool progress = true;
            for(;;)
            {
                error_code ec;
                ds.write(zs, Flush::full, ec);
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
        out.resize(zs.total_out);
        BEAST_EXPECT(decompress(out) == check);

    err:
        ;
    }

    //--------------------------------------------------------------------------

    void
    doDeflate2_beast(
        int level, int windowBits, int memLevel,
        int strategy, std::string const& check)
    {
        for(std::size_t i = 1; i < check.size(); ++i)
        {
            for(std::size_t j = 1;; ++j)
            {
                z_params zs;
                deflate_stream ds;
                ds.reset(
                    level,
                    windowBits,
                    memLevel,
                    toStrategy(strategy));
                std::string out;
                out.resize(ds.upper_bound(
                    static_cast<uLong>(check.size())));
                if(j >= out.size())
                    break;
                zs.next_in = (Bytef*)check.data();
                zs.avail_in = static_cast<uInt>(i);
                zs.next_out = (Bytef*)out.data();
                zs.avail_out = static_cast<uInt>(j);
                bool bi = false;
                bool bo = false;
                for(;;)
                {
                    error_code ec;
                    ds.write(zs,
                        bi ? Flush::full : Flush::none, ec);
                    if( ec == error::need_buffers ||
                        ec == error::end_of_stream) // per zlib FAQ
                        goto fin;
                    if(! BEAST_EXPECTS(! ec, ec.message()))
                        goto err;
                    if(zs.avail_in == 0 && ! bi)
                    {
                        bi = true;
                        zs.avail_in =
                            static_cast<uInt>(check.size() - i);
                    }
                    if(zs.avail_out == 0 && ! bo)
                    {
                        bo = true;
                        zs.avail_out =
                            static_cast<uInt>(out.size() - j);
                    }
                }

            fin:
                out.resize(zs.total_out);
                BEAST_EXPECT(decompress(out) == check);

            err:
                ;
            }
        }
    }

    //--------------------------------------------------------------------------

    void
    doMatrix(std::string const& check, pmf_t pmf)
    {
        for(int level = 0; level <= 9; ++level)
        {
            for(int windowBits = 8; windowBits <= 9; ++windowBits)
            {
                // zlib has a bug with windowBits==8
                if(windowBits == 8)
                    continue;
                for(int strategy = 0; strategy <= 4; ++strategy)
                {
                    for (int memLevel = 8; memLevel <= 9; ++memLevel)
                    {
                        (this->*pmf)(
                                level, windowBits, memLevel, strategy, check);
                    }
                }
            }
        }

        // Check default settings
        (this->*pmf)(compression::default_size, 15, 8, 0, check);
    }

    void
    testDeflate()
    {
        doMatrix("Hello, world!", &self::doDeflate1_beast);
        doMatrix("Hello, world!", &self::doDeflate2_beast);
        doMatrix(corpus1(56), &self::doDeflate2_beast);
        doMatrix(corpus1(1024), &self::doDeflate1_beast);
    }

    void testInvalidSettings()
    {
        except<std::invalid_argument>(
            []()
            {
                deflate_stream ds;
                ds.reset(-42, 15, 8, Strategy::normal);
            });
        except<std::invalid_argument>(
            []()
            {
                deflate_stream ds;
                ds.reset(compression::default_size, -1, 8, Strategy::normal);
            });
        except<std::invalid_argument>(
            []()
            {
                deflate_stream ds;
                ds.reset(compression::default_size, 15, -1, Strategy::normal);
            });
        except<std::invalid_argument>(
            []()
            {
                deflate_stream ds;
                ds.reset();
                z_params zp{};
                zp.avail_in = 1;
                zp.next_in = nullptr;
                error_code ec;
                ds.write(zp, Flush::full, ec);
            });
    }

    void
    testWriteAfterFinish()
    {
        z_params zp;
        deflate_stream ds;
        ds.reset();
        std::string out;
        out.resize(1024);
        string_view s = "Hello";
        zp.next_in = s.data();
        zp.avail_in = s.size();
        zp.next_out = &out.front();
        zp.avail_out = out.size();
        error_code ec;
        ds.write(zp, Flush::sync, ec);
        BEAST_EXPECT(!ec);
        zp.next_in = nullptr;
        zp.avail_in = 0;
        ds.write(zp, Flush::finish, ec);
        BEAST_EXPECT(ec == error::end_of_stream);
        zp.next_in = s.data();
        zp.avail_in = s.size();
        zp.next_out = &out.front();
        zp.avail_out = out.size();
        ds.write(zp, Flush::sync, ec);
        BEAST_EXPECT(ec == error::stream_error);
        ds.write(zp, Flush::finish, ec);
        BEAST_EXPECT(ec == error::need_buffers);
    }

    void
    testFlushPartial()
    {
        z_params zp;
        deflate_stream ds;
        ds.reset();
        std::string out;
        out.resize(1024);
        string_view s = "Hello";
        zp.next_in = s.data();
        zp.avail_in = s.size();
        zp.next_out = &out.front();
        zp.avail_out = out.size();
        error_code ec;
        ds.write(zp, Flush::none, ec);
        BEAST_EXPECT(!ec);
        ds.write(zp, Flush::partial, ec);
        BEAST_EXPECT(!ec);
    }

    void
    testFlushAtLiteralBufferFull()
    {
        struct fixture
        {
            explicit fixture(std::size_t n, Strategy s)
            {
                ds.reset(8, 15, 1, s);
                std::iota(in.begin(), in.end(), 0);
                out.resize(n);
                zp.next_in = in.data();
                zp.avail_in = in.size();
                zp.next_out = &out.front();
                zp.avail_out = out.size();
            }

            z_params zp;
            deflate_stream ds;
            std::array<std::uint8_t, 255> in;
            std::string out;
        };

        for (auto s : {Strategy::huffman, Strategy::rle, Strategy::normal})
        {
            {
                fixture f{264, s};
                error_code ec;
                f.ds.write(f.zp, Flush::finish, ec);
                BEAST_EXPECT(ec == error::end_of_stream);
                BEAST_EXPECT(f.zp.avail_out == 1);
            }

            {
                fixture f{263, s};
                error_code ec;
                f.ds.write(f.zp, Flush::finish, ec);
                BEAST_EXPECT(!ec);
                BEAST_EXPECT(f.zp.avail_out == 0);
            }

            {
                fixture f{20, s};
                error_code ec;
                f.ds.write(f.zp, Flush::sync, ec);
                BEAST_EXPECT(!ec);
            }

        }
    }

    void
    testRLEMatchLengthExceedLookahead()
    {
        z_params zp;
        deflate_stream ds;
        std::vector<std::uint8_t> in;
        in.resize(300);


        ds.reset(8, 15, 1, Strategy::rle);
        std::fill_n(in.begin(), 4, 'a');
        std::string out;
        out.resize(in.size() * 2);
        zp.next_in = in.data();
        zp.avail_in = in.size();
        zp.next_out = &out.front();
        zp.avail_out = out.size();

        error_code ec;
        ds.write(zp, Flush::sync, ec);
        BEAST_EXPECT(!ec);
    }

    void
    run() override
    {
        log <<
            "sizeof(deflate_stream) == " <<
            sizeof(deflate_stream) << std::endl;

        testDeflate();
        testInvalidSettings();
        testWriteAfterFinish();
        testFlushPartial();
        testFlushAtLiteralBufferFull();
        testRLEMatchLengthExceedLookahead();
    }
};

BEAST_DEFINE_TESTSUITE(beast,zlib,deflate_stream);

} // zlib
} // beast
} // boost
