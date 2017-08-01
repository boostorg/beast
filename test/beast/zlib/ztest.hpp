//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_ZTEST_HPP
#define BOOST_BEAST_ZTEST_HPP

#include "zlib-1.2.11/zlib.h"

#include <string>

class z_deflator
{
    int level_      = Z_DEFAULT_COMPRESSION;
    int windowBits_ = 15;
    int memLevel_   = 4;
    int strategy_   = Z_DEFAULT_STRATEGY;

public:
    // -1    = default
    //  0    = none
    //  1..9 = faster<-->better
    void
    level(int n)
    {
        level_ = n;
    }

    void
    windowBits(int n)
    {
        windowBits_ = n;
    }

    void
    memLevel(int n)
    {
        memLevel_ = n;
    }

    void
    strategy(int n)
    {
        strategy_ = n;
    }

    std::string
    operator()(std::string const& in)
    {
        int result;
        z_stream zs;
        memset(&zs, 0, sizeof(zs));
        result = deflateInit2(
            &zs,
            level_,
            Z_DEFLATED,
            -windowBits_,
            memLevel_,
            strategy_
        );
        if(result != Z_OK)
            throw std::logic_error("deflateInit2 failed");
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
};

#endif
