//
// Copyright (c) 2016-2020 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2020 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_DETAIL_STREAM_UTILS_HPP
#define BOOST_BEAST_DETAIL_STREAM_UTILS_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/dynamic_buffer.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/coroutine.hpp>
#include <type_traits>

namespace boost {
namespace beast {
namespace detail {

// Implements the "grow, read-some, shrink" idiom
// required by read_some operations with DynamicBuffer_v2 buffers
template<
    class SyncReadStream,
    class DynamicBuffer_v2>
std::size_t
dynamic_read_some(
    SyncReadStream& stream,
    DynamicBuffer_v2 buffer,
    std::size_t grow_size,
    error_code& ec
    )
{
    auto pos = buffer.size();
    buffer.grow(grow_size);
    auto rb = buffer.data(pos, grow_size);
    auto bytes_transferred = stream.read_some(rb, ec);
    buffer.shrink(grow_size - bytes_transferred);
    return bytes_transferred;
}

template<class AsyncReadStream,
    class DynamicBuffer_v2>
struct async_dynamic_read_some_op
    : boost::asio::coroutine
{
    AsyncReadStream& stream;
    DynamicBuffer_v2 buffer;
    std::size_t grow_size;

    async_dynamic_read_some_op(
        AsyncReadStream& stream,
        DynamicBuffer_v2 buffer,
        std::size_t grow_size)
    : stream(stream)
    , buffer(buffer)
    , grow_size(grow_size)
    {
    }

#include <boost/asio/yield.hpp>
    template<class Self>
    void operator()(
        Self& self,
        error_code const& ec = {},
        std::size_t bytes_transferred = 0)
    {
        reenter(this)
        {
            yield
            {
                auto pos = buffer.size();
                buffer.grow(grow_size);
                auto rb = buffer.data(pos, grow_size);
                stream.async_read_some(rb, std::move(self));
            }
            buffer.shrink(grow_size - bytes_transferred);
            self.complete(ec, bytes_transferred);
        }
    }
#include <boost/asio/unyield.hpp>

};

// Implements the "grow, read-some, shrink" idiom
// required by async_read_some operations with DynamicBuffer_v2 buffers
template<class AsyncReadStream,
    class DynamicBuffer_v2,
        class CompletionToken>
auto async_dynamic_read_some(
    AsyncReadStream& stream,
    DynamicBuffer_v2 buffer,
    std::size_t grow_size,
    CompletionToken&& token)
    ->
typename boost::asio::async_result<
    typename std::decay<CompletionToken>::type,
    void(error_code, std::size_t)>::return_type
{
    return net::async_compose<CompletionToken,
        void(error_code, std::size_t)>(
            async_dynamic_read_some_op<
                AsyncReadStream,
                DynamicBuffer_v2>(
                    stream,
                    buffer,
                    grow_size),
            token,
            stream);
}


} // detail
} // beast
} // boost

#endif
