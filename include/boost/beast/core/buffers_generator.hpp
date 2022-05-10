//
// Copyright (c) 2020 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_BUFFERS_GENERATOR_HPP
#define BOOST_BEAST_CORE_BUFFERS_GENERATOR_HPP

#include <boost/beast/core/detail/config.hpp>

#include <boost/asio/write.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/coroutine.hpp>

#include <boost/beast/core/buffer_traits.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/core/detail/type_traits.hpp>

#include <boost/throw_exception.hpp>
#include <type_traits>

namespace boost {
namespace beast {

/** Determine if type satisfies the <em>BuffersGenerator</em> requirements.

    This metafunction is used to determine if the specified type meets the
    requirements for a buffers generator.

    The static member `value` will evaluate to `true` if so, `false` otherwise.

    @tparam T a type to check
*/
#ifdef BOOST_BEAST_DOXYGEN
template <class T>
struct is_buffers_generator
    : integral_constant<bool, automatically_determined>
{
};
#else
template<class T, class = void>
struct is_buffers_generator
    : std::false_type
{
};

template<class T>
struct is_buffers_generator<
    T, detail::void_t<decltype(
        typename T::const_buffers_type(
            std::declval<T&>().prepare(
                std::declval<error_code&>())),
        std::declval<T&>().consume(
            std::size_t{})
    )>> : std::true_type
{
};
#endif

//----------------------------------------------------------

namespace detail {

template <
    class AsyncWriteStream,
    class BuffersGenerator>
struct write_buffers_generator_op
    : boost::asio::coroutine
{
    write_buffers_generator_op(AsyncWriteStream& stream,
                               BuffersGenerator generator)
        : stream_(stream)
        , generator_(std::move(generator))
    {
    }

    template <class Self>
    void operator()(Self& self,
                    error_code ec = {},
                    std::size_t n = 0)
    {
        BOOST_ASIO_CORO_REENTER(*this)
        {
            for(;;)
            {
                BOOST_ASIO_CORO_YIELD
                {
                    auto cb = generator_.prepare(ec);
                    if (ec)
                        goto complete;
                    if (boost::beast::buffer_bytes(cb) == 0)
                        goto complete;
                    stream_.async_write_some(
                        cb, std::move(self));
                }
                generator_.consume(n);

                total_ += n;
            }

        complete:
            self.complete(ec, total_);
        }
    }

private:
    AsyncWriteStream& stream_;
    BuffersGenerator generator_;
    std::size_t total_ = 0;
};

} // detail

//----------------------------------------------------------

template <
    class SyncWriteStream,
    class BuffersGenerator
#if ! BOOST_BEAST_DOXYGEN
    ,
    typename std::enable_if<
        is_sync_write_stream<SyncWriteStream>::value &&
        is_buffers_generator<BuffersGenerator>::value>::
        type* = nullptr
#endif
    >
size_t
write(SyncWriteStream& stream,
      BuffersGenerator generator,
      beast::error_code& ec)
{
    ec.clear();
    size_t total = 0;
    for (;;) {
        auto cb = generator.prepare(ec);
        if (ec)
            break;
        if (boost::beast::buffer_bytes(cb) == 0)
            break;

        size_t n = net::write(stream, cb, ec);

        if (ec)
            break;

        generator.consume(n);
        total += n;
    }

    return total;
}

//----------------------------------------------------------

template <
    class SyncWriteStream,
    class BuffersGenerator
#if ! BOOST_BEAST_DOXYGEN
    ,
    typename std::enable_if<
        is_sync_write_stream<SyncWriteStream>::value &&
        is_buffers_generator<BuffersGenerator>::value>::
        type* = nullptr
#endif
    >
std::size_t
write(SyncWriteStream& stream, BuffersGenerator generator)
{
    beast::error_code ec;
    std::size_t n = write(stream, generator, ec);
    if (ec)
        BOOST_THROW_EXCEPTION(system_error{ ec });
    return n;
}

//----------------------------------------------------------

template <
    class AsyncWriteStream,
    class BuffersGenerator,
    class CompletionToken
#if !BOOST_BEAST_DOXYGEN
    ,
    typename std::enable_if<
        is_async_write_stream<AsyncWriteStream>::value &&
        is_buffers_generator<BuffersGenerator>::value //
        >::type* = nullptr
#endif
    >
auto
async_write(AsyncWriteStream& stream,
            BuffersGenerator generator,
            CompletionToken&& token) ->
    typename net::async_result<
        typename std::decay<CompletionToken>::type,
        void(error_code, std::size_t)>::return_type
{
    static_assert(
        is_async_write_stream<AsyncWriteStream>::value,
        "AsyncWriteStream type requirements not met");
    return net::async_compose< //
        CompletionToken,
        void(error_code, std::size_t)>(
        detail::write_buffers_generator_op<
            AsyncWriteStream,
            BuffersGenerator>{ stream,
                               std::move(generator) },
        token, // REVIEW: why doesn't forwarding work here?
               // SEHE: do we want to take `token` by value?
        stream);
}

} // namespace beast
} // namespace boost

#endif // BOOST_BEAST_CORE_BUFFERS_GENERATOR_HPP
