//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_WEBSOCKET_ZIO_HPP
#define BEAST_WEBSOCKET_ZIO_HPP

#include <beast/core/error.hpp>
#include <boost/asio/buffer.hpp>

#include "../test/zlib/zlib-1.2.8/zlib.h"

namespace beast {
namespace websocket {
namespace detail {

class zinflate_stream
{
    ::z_stream zs_;

public:
    zinflate_stream()
    {
        std::memset(&zs_, 0, sizeof(zs_));
        inflateInit2(&zs_, -15);
    }

    void
    reset(int windowBits)
    {
        inflateReset2(&zs_, -windowBits);
    }

    void
    reset()
    {
        inflateReset(&zs_);
    }

    void
    write(zlib::z_params& zs, zlib::Flush flush, error_code& ec)
    {
        zs_.next_in = (Bytef*)zs.next_in;
        zs_.next_out = (Bytef*)zs.next_out;
        zs_.avail_in = zs.avail_in;
        zs_.avail_out = zs.avail_out;
        zs_.total_in = zs.total_in;
        zs_.total_out = zs.total_out;

        auto const result = inflate(
            &zs_, Z_SYNC_FLUSH);

        switch(result)
        {
        case Z_BUF_ERROR: ec = zlib::error::need_buffers; break;
        case Z_STREAM_END: ec = zlib::error::end_of_stream; break;
        case Z_OK: break;
        default:
            ec = zlib::error::stream_error;
            break;
        }

        zs.next_in =    zs_.next_in;
        zs.next_out =   zs_.next_out;
        zs.avail_in =   zs_.avail_in;
        zs.avail_out =  zs_.avail_out;
        zs.total_in =   zs_.total_in;
        zs.total_out =  zs_.total_out;
    }

    ~zinflate_stream()
    {
        inflateEnd(&zs_);
    }
};

class zdeflate_stream
{
    ::z_stream zs_;

public:
    zdeflate_stream()
    {
        std::memset(&zs_, 0, sizeof(zs_));
        deflateInit2(&zs_,
            Z_DEFAULT_COMPRESSION,
            Z_DEFLATED,
            -15,
            4,
            Z_DEFAULT_STRATEGY);
    }

    ~zdeflate_stream()
    {
        deflateEnd(&zs_);
    }

    void
    reset()
    {
        deflateReset(&zs_);
    }

    void
    reset(
        int compLevel,
        int windowBits,
        int memLevel,
        zlib::Strategy strategy)
    {
        deflateEnd(&zs_);
        deflateInit2(&zs_,
            Z_DEFAULT_COMPRESSION,
            Z_DEFLATED,
            -windowBits,
            4,
            Z_DEFAULT_STRATEGY);
    }

    void
    write(zlib::z_params& zs, zlib::Flush flush, error_code& ec)
    {
        zs_.next_in = (Bytef*)zs.next_in;
        zs_.next_out = (Bytef*)zs.next_out;
        zs_.avail_in = zs.avail_in;
        zs_.avail_out = zs.avail_out;
        zs_.total_in = zs.total_in;
        zs_.total_out = zs.total_out;

        int fl;
        switch(flush)
        {
        case zlib::Flush::none: fl = Z_NO_FLUSH; break;
        case zlib::Flush::full: fl = Z_FULL_FLUSH; break;
        case zlib::Flush::block: fl = Z_BLOCK; break;
        default:
            throw std::invalid_argument{"unknown flush"};
        }
        auto const result = deflate(&zs_, fl);

        switch(result)
        {
        case Z_BUF_ERROR: ec = zlib::error::need_buffers; break;
        case Z_STREAM_END: ec = zlib::error::end_of_stream; break;
        case Z_OK: break;
        default:
            ec = zlib::error::stream_error;
            break;
        }

        zs.next_in =    zs_.next_in;
        zs.next_out =   zs_.next_out;
        zs.avail_in =   zs_.avail_in;
        zs.avail_out =  zs_.avail_out;
        zs.total_in =   zs_.total_in;
        zs.total_out =  zs_.total_out;
    }
};

} // detail
} // websocket
} // beast

#endif
