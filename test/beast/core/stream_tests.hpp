//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_STREAM_TESTS_HPP
#define BOOST_BEAST_STREAM_TESTS_HPP

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/use_future.hpp>

namespace boost {
namespace beast {

template<class AsyncReadStream>
void
test_async_read_stream()
{
    struct handler
    {
        void operator()(error_code, std::size_t)
        {
        }
    };

    BOOST_ASSERT(is_async_read_stream<AsyncReadStream>::value);
    BEAST_EXPECT(&AsyncReadStream::get_executor);
    BEAST_EXPECT((&AsyncReadStream::template async_read_some<net::mutable_buffer, handler>));
    BEAST_EXPECT((&AsyncReadStream::template async_read_some<net::mutable_buffer, net::use_future_t<>>));
    BEAST_EXPECT((&AsyncReadStream::template async_read_some<net::mutable_buffer, net::yield_context>));
}

template<class AsyncWriteStream>
void
test_async_write_stream()
{
    struct handler
    {
        void operator()(error_code, std::size_t)
        {
        }
    };

    BOOST_ASSERT(is_async_write_stream<AsyncWriteStream>::value);
    BEAST_EXPECT(&AsyncWriteStream::get_executor);
    BEAST_EXPECT((&AsyncWriteStream::template async_write_some<net::const_buffer, handler>));
    BEAST_EXPECT((&AsyncWriteStream::template async_write_some<net::const_buffer, net::use_future_t<>>));
    BEAST_EXPECT((&AsyncWriteStream::template async_write_some<net::const_buffer, net::yield_context>));
}

template<class AsyncStream>
void
test_async_stream()
{
    test_async_read_stream<AsyncStream>();
    test_async_write_stream<AsyncStream>();
}

} // beast
} // boost

#endif
