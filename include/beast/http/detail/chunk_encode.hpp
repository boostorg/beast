//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_DETAIL_CHUNK_ENCODE_HPP
#define BEAST_HTTP_DETAIL_CHUNK_ENCODE_HPP

#include <boost/asio/buffer.hpp>
#include <algorithm>
#include <array>
#include <cstddef>

namespace beast {
namespace http {
namespace detail {

/** A buffer sequence containing a chunk-encoding header
*/
class chunk_header
{
public:
    // Storage for the longest hex string we might need
    class value_type
    {
        friend class chunk_header;

        // First byte holds the length
        char buf_[1 + 2 * sizeof(std::size_t)];

        template<class = void>
        void
        prepare(std::size_t n);

        template<class OutIter>
        static
        OutIter
        to_hex(OutIter last, std::size_t n)
        {
            if(n == 0)
            {
                *--last = '0';
                return last;
            }
            while(n)
            {
                *--last = "0123456789abcdef"[n&0xf];
                n>>=4;
            }
            return last;
        }
    public:
        operator
        boost::asio::const_buffer() const
        {
            return {
                buf_ + sizeof(buf_) - buf_[0],
                static_cast<unsigned>(buf_[0])};
        }
    };

    using const_iterator = value_type const*;

    chunk_header(chunk_header const& other) = default;

    /** Construct a chunk header

        @param n The number of octets in this chunk.
    */
    explicit
    chunk_header(std::size_t n)
    {
        value_.prepare(n);
    }

    const_iterator
    begin() const
    {
        return &value_;
    }

    const_iterator
    end() const
    {
        return begin() + 1;
    }

private:
    value_type value_;
};

template<class>
void
chunk_header::
value_type::
prepare(std::size_t n)
{
    auto const last = &buf_[sizeof(buf_)];
    auto it = to_hex(last, n);
    buf_[0] = static_cast<char>(last - it);
}

/// Returns a buffer sequence holding a CRLF for chunk encoding
inline
boost::asio::const_buffers_1
chunk_crlf()
{
    return {"\r\n", 2};
}

/// Returns a buffer sequence holding a final chunk header
inline
boost::asio::const_buffers_1
chunk_final()
{
    return {"0\r\n", 3};
}

} // detail
} // http
} // beast

#endif
