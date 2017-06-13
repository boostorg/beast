//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_EXAMPLE_SERVER_SSL_STREAM_HPP
#define BEAST_EXAMPLE_SERVER_SSL_STREAM_HPP

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <memory>

namespace beast {

/** Movable SSL socket wrapper

    This wrapper provides an interface identical to `boost::asio::ssl::stream`,
    which is additionally move constructible and move assignable.
*/
template<class NextLayer>
class ssl_stream
    : public boost::asio::ssl::stream_base
{
    using stream_type = boost::asio::ssl::stream<NextLayer>;

    std::unique_ptr<stream_type> p_;

public:
    /// The native handle type of the SSL stream.
    using native_handle_type = typename stream_type::native_handle_type;

    /// Structure for use with deprecated impl_type.
    using impl_struct = typename stream_type::impl_struct;

    /// (Deprecated: Use native_handle_type.) The underlying implementation type.
    using impl_type = typename stream_type::impl_type;

    /// The type of the next layer.
    using next_layer_type = typename stream_type::next_layer_type;

    /// The type of the lowest layer.
    using lowest_layer_type = typename stream_type::lowest_layer_type;

    ssl_stream(ssl_stream&&) = default;
    ssl_stream(ssl_stream const&) = delete;
    ssl_stream& operator=(ssl_stream&&) = default;
    ssl_stream& operator=(ssl_stream const&) = delete;

    template<class... Args>
    ssl_stream(Args&&... args)
        : p_(new stream_type{std::forward<Args>(args)...)
    {
    }

    boost::asio::io_service&
    get_io_service()
    {
        return p_->get_io_service();
    }

    native_handle_type
    native_handle()
    {
        return p_->native_handle();
    }

    impl_type
    impl()
    {
        return p_->impl();
    }

    next_layer_type const&
    next_layer() const
    {
        return p_->next_layer();
    }

    next_layer_type&
    next_layer()
    {
        return p_->next_layer();
    }

    lowest_layer_type&
    lowest_layer()
    {
        return p_->lowest_layer();
    }

    lowest_layer_type const&
    lowest_layer() const
    {
        return p_->lowest_layer();
    }

    void
    set_verify_mode(boost::asio::ssl::verify_mode v)
    {
        p_->set_verify_mode(v);
    }

    boost::system::error_code
    set_verify_mode(boost::asio::ssl::verify_mode v,
        boost::system::error_code& ec)
    {
        return p_->set_verify_mode(v, ec);
    }

    void
    set_verify_depth(int depth)
    {
        p_->set_verify_depth(depth);
    }

    boost::system::error_code
    set_verify_depth(
        int depth, boost::system::error_code& ec)
    {
        return p_->set_verify_depth(depth, ec);
    }

    template<class VerifyCallback>
    void
    set_verify_callback(VerifyCallback callback)
    {
        p_->set_verify_callback(callback);
    }

    template<class VerifyCallback>
    boost::system::error_code
    set_verify_callback(VerifyCallback callback,
        boost::system::error_code& ec)
    {
        return p_->set_verify_callback(callback, ec);
    }

    void
    handshake(handshake_type type)
    {
        p_->handshake(type);
    }

    boost::system::error_code
    handshake(handshake_type type,
        boost::system::error_code& ec)
    {
        return p_->handshake(type, ec);
    }

    template<class ConstBufferSequence>
    void
    handshake(
        handshake_type type, ConstBufferSequence const& buffers)
    {
        p_->handshake(type, buffers);
    }

    template<class ConstBufferSequence>
    boost::system::error_code
    handshake(handshake_type type,
        ConstBufferSequence const& buffers,
            boost::system::error_code& ec)
    {
        return p_->handshake(type, buffers, ec);
    }

    template<class HandshakeHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(HandshakeHandler,
        void(boost::system::error_code))
    async_handshake(handshake_type type,
        BOOST_ASIO_MOVE_ARG(HandshakeHandler) handler)
    {
        return p_->async_handshake(type,
            BOOST_ASIO_MOVE_CAST(HandshakeHandler)(handler));
    }

    template<class ConstBufferSequence, class BufferedHandshakeHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(BufferedHandshakeHandler,
        void (boost::system::error_code, std::size_t))
    async_handshake(handshake_type type, ConstBufferSequence const& buffers,
        BOOST_ASIO_MOVE_ARG(BufferedHandshakeHandler) handler)
    {
        return p_->async_handshake(type, buffers,
            BOOST_ASIO__MOVE_CAST(BufferedHandshakeHandler)(handler));
    }

    void
    shutdown()
    {
        p_->shutdown();
    }

    boost::system::error_code
    shutdown(boost::system::error_code& ec)
    {
        return p_->shutdown(ec);
    }

    template<class ShutdownHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(ShutdownHandler,
        void (boost::system::error_code))
    async_shutdown(BOOST_ASIO_MOVE_ARG(ShutdownHandler) handler)
    {
        return p_->async_shutdown(
            BOOST_ASIO_MOVE_CAST(ShutdownHandler)(handler));
    }

    template<class ConstBufferSequence>
    std::size_t
    write_some(ConstBufferSequence const& buffers)
    {
        return p_->write_some(buffers);
    }

    template<class ConstBufferSequence>
    std::size_t
    write_some(ConstBufferSequence const& buffers,
        boost::system::error_code& ec)
    {
        return p_->write_some(buffers, ec);
    }

    template<class ConstBufferSequence, class WriteHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(WriteHandler,
        void (boost::system::error_code, std::size_t))
    async_write_some(ConstBufferSequence const& buffers,
        BOOST_ASIO_MOVE_ARG(WriteHandler) handler)
    {
        return p_->async_write_some(buffers,
            BOOST_ASIO_MOVE_CAST(WriteHandler)(handler));
    }

    template<class MutableBufferSequence>
    std::size_t
    read_some(MutableBufferSequence const& buffers)
    {
        return p_->read_some(buffers);
    }

    template<class MutableBufferSequence>
    std::size_t
    read_some(MutableBufferSequence const& buffers,
        boost::system::error_code& ec)
    {
        return p_->read_some(buffers, ec);
    }

    template<class MutableBufferSequence, class ReadHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(ReadHandler,
        void(boost::system::error_code, std::size_t))
    async_read_some(MutableBufferSequence& buffers,
        BOOST_ASIO_MOVE_ARG(ReadHandler) handler)
    {
        return p_->async_read_some(buffers,
            BOOST_ASIO_MOVE_CAST(ReadHandler)(handler))
    }
};

} // beast

#endif