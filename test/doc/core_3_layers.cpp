//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include "snippets.hpp"

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/core/async_op_base.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/detail/get_executor_type.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/async_result.hpp>
#include <cstdlib>
#include <utility>

namespace boost {
namespace beast {

namespace {

void
snippets()
{
    #include "snippets.ipp"
    {
    //[code_core_3_layers_1

        net::ssl::stream<net::ip::tcp::socket> ss(ioc, ctx);

    //]
    }
    {
    //[code_core_3_layers_2

        websocket::stream<net::ip::tcp::socket> ws(ioc);

    //]
    }
    //[code_core_3_layers_3

        websocket::stream<net::ssl::stream<net::ip::tcp::socket>> ws(ioc, ctx);

    //]
}

//[code_core_3_layers_4

// Set non-blocking mode on a stack of stream
// layers with a regular socket at the lowest layer.
template <class Stream>
void set_non_blocking (Stream& stream)
{
    error_code ec;
    // A compile error here means your lowest layer is not the right type!
    get_lowest_layer(stream).non_blocking(true, ec);
    if(ec)
        throw system_error{ec};
}

//]

//[code_core_3_layers_5

// A layered stream which counts the bytes read and bytes written on the next layer
template <class NextLayer>
class counted_stream
{
    NextLayer next_layer_;
    std::size_t bytes_read_ = 0;
    std::size_t bytes_written_ = 0;

public:
    /// The type of executor used by this stream
    using executor_type = detail::get_executor_type<NextLayer>;

    /// Constructor
    template <class... Args>
    explicit
    counted_stream(Args&&... args)
        : next_layer_(std::forward<Args>(args)...)
    {
    }

    /// Returns an instance of the executor used to submit completion handlers
    executor_type get_executor() noexcept
    {
        return next_layer_.get_executor();
    }

    /// Returns a reference to the next layer
    NextLayer& next_layer() noexcept
    {
        return next_layer_;
    }

    /// Returns a reference to the next layer
    NextLayer const& next_layer() const noexcept
    {
        return next_layer_;
    }

    /// Returns the total number of bytes read since the stream was constructed
    std::size_t bytes_read() const noexcept
    {
        return bytes_read_;
    }

    /// Returns the total number of bytes written since the stream was constructed
    std::size_t bytes_written() const noexcept
    {
        return bytes_written_;
    }

    /// Read some data from the stream
    template <class MutableBufferSequence>
    std::size_t read_some(MutableBufferSequence const& buffers)
    {
        auto const bytes_transferred = next_layer_.read_some(buffers);
        bytes_read_ += bytes_transferred;
        return bytes_transferred;
    }

    /// Read some data from the stream
    template <class MutableBufferSequence>
    std::size_t read_some(MutableBufferSequence const& buffers, error_code& ec)
    {
        auto const bytes_transferred = next_layer_.read_some(buffers, ec);
        bytes_read_ += bytes_transferred;
        return bytes_transferred;
    }

    /// Write some data to the stream
    template <class ConstBufferSequence>
    std::size_t write_some(ConstBufferSequence const& buffers)
    {
        auto const bytes_transferred = next_layer_.write_some(buffers);
        bytes_written_ += bytes_transferred;
        return bytes_transferred;
    }

    /// Write some data to the stream
    template <class ConstBufferSequence>
    std::size_t write_some(ConstBufferSequence const& buffers, error_code& ec)
    {
        auto const bytes_transferred = next_layer_.write_some(buffers, ec);
        bytes_written_ += bytes_transferred;
        return bytes_transferred;
    }

    /// Read some data from the stream asynchronously
    template<class MutableBufferSequence, class ReadHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(ReadHandler,
        void(error_code, std::size_t))
    async_read_some(
        MutableBufferSequence const& buffers,
        ReadHandler&& handler)
    {
        using handler_type = BOOST_ASIO_HANDLER_TYPE(
            ReadHandler, void(error_code, std::size_t));
        struct op : async_op_base<handler_type, executor_type>
        {
            counted_stream& stream_;

            op(
                counted_stream& stream,
                handler_type&& handler,
                MutableBufferSequence const& buffers)
                : async_op_base<handler_type, executor_type>(
                    std::move(handler), stream.get_executor())
                , stream_(stream)
            {
                stream_.next_layer().async_read_some(buffers, std::move(*this));
            }

            void operator()(error_code ec, std::size_t bytes_transferred)
            {
                stream_.bytes_read_ += bytes_transferred;
                this->invoke(ec, bytes_transferred);
            }
        };
        net::async_completion<ReadHandler, void(error_code, std::size_t)> init{handler};
        op(*this, std::move(init.completion_handler), buffers);
        return init.result.get();
    }

    /// Write some data to the stream asynchronously
    template<class ConstBufferSequence, class WriteHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(WriteHandler,
        void(error_code, std::size_t))
    async_write_some(
        ConstBufferSequence const& buffers,
        WriteHandler&& handler)
    {
        using handler_type = BOOST_ASIO_HANDLER_TYPE(
            WriteHandler, void(error_code, std::size_t));
        struct op : async_op_base<handler_type, executor_type>
        {
            counted_stream& stream_;

            op( counted_stream& stream,
                handler_type&& handler,
                ConstBufferSequence const& buffers)
                : async_op_base<handler_type, executor_type>(
                    std::move(handler), stream.get_executor())
                , stream_(stream)
            {
                stream_.next_layer().async_write_some(buffers, std::move(*this));
            }

            void operator()(error_code ec, std::size_t bytes_transferred)
            {
                stream_.bytes_written_ += bytes_transferred;
                this->invoke(ec, bytes_transferred);
            }
        };
        net::async_completion<WriteHandler, void(error_code, std::size_t)> init{handler};
        op(*this, std::move(init.completion_handler), buffers);
        return init.result.get();
    }
};
//]

BOOST_STATIC_ASSERT(is_sync_read_stream<counted_stream<test::stream>>::value);
BOOST_STATIC_ASSERT(is_sync_write_stream<counted_stream<test::stream>>::value);
BOOST_STATIC_ASSERT(is_async_read_stream<counted_stream<test::stream>>::value);
BOOST_STATIC_ASSERT(is_async_write_stream<counted_stream<test::stream>>::value);

} // (anon)

struct core_3_layers_test
    : public beast::unit_test::suite
{
    struct handler
    {
        void operator()(error_code, std::size_t)
        {
        }
    };

    void
    run() override
    {
        BEAST_EXPECT(&snippets);
        BEAST_EXPECT(&set_non_blocking<net::ip::tcp::socket>);

        BEAST_EXPECT(&counted_stream<test::stream>::get_executor);
        BEAST_EXPECT(static_cast<
            test::stream&(counted_stream<test::stream>::*)()>(
            &counted_stream<test::stream>::next_layer));
        BEAST_EXPECT(static_cast<
            test::stream const&(counted_stream<test::stream>::*)() const>(
            &counted_stream<test::stream>::next_layer));
        BEAST_EXPECT(&counted_stream<test::stream>::bytes_read);
        BEAST_EXPECT(&counted_stream<test::stream>::bytes_written);
        BEAST_EXPECT(static_cast<
            std::size_t(counted_stream<test::stream>::*)(net::mutable_buffer const&)>(
            &counted_stream<test::stream>::read_some));
        BEAST_EXPECT(static_cast<
            std::size_t(counted_stream<test::stream>::*)(net::mutable_buffer const&, error_code&)>(
            &counted_stream<test::stream>::read_some));
        BEAST_EXPECT(static_cast<
            std::size_t(counted_stream<test::stream>::*)(net::const_buffer const&)>(
            &counted_stream<test::stream>::write_some));
        BEAST_EXPECT(static_cast<
            std::size_t(counted_stream<test::stream>::*)(net::const_buffer const&, error_code&)>(
            &counted_stream<test::stream>::write_some));
        BEAST_EXPECT((&counted_stream<test::stream>::async_read_some<net::mutable_buffer, handler>));
        BEAST_EXPECT((&counted_stream<test::stream>::async_write_some<net::const_buffer, handler>));
    }
};

BEAST_DEFINE_TESTSUITE(beast,doc,core_3_layers);

} // beast
} // boost
