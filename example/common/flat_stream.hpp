//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_EXAMPLE_COMMON_FLAT_STREAM_HPP
#define BOOST_BEAST_EXAMPLE_COMMON_FLAT_STREAM_HPP

#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/handler_ptr.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/websocket/teardown.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/throw_exception.hpp>
#include <iterator>
#include <memory>
#include <utility>

namespace detail {

class flat_stream_base
{
public:
    static std::size_t constexpr coalesce_limit = 64 * 1024;

    // calculates the coalesce settings for a buffer sequence
    template<class BufferSequence>
    static
    std::pair<std::size_t, bool>
    coalesce(BufferSequence const& buffers, std::size_t limit)
    {
        std::pair<std::size_t, bool> result{0, false};
        auto first = boost::asio::buffer_sequence_begin(buffers);
        auto last = boost::asio::buffer_sequence_end(buffers);
        if(first != last)
        {
            result.first = boost::asio::buffer_size(*first);
            if(result.first < limit)
            {
                auto it = first;
                auto prev = first;
                while(++it != last)
                {
                    auto const n =
                        boost::asio::buffer_size(*it);
                    if(result.first + n > limit)
                        break;
                    result.first += n;
                    prev = it;
                }
                result.second = prev != first;
            }
        }
        return result;
    }
};

} // detail

/** Flat stream wrapper.

    This wrapper flattens buffer sequences having length greater than 1
    and total size below a predefined amount, using a dynamic memory
    allocation. It is primarily designed to overcome a performance
    limitation of the current Boost.Asio version of ssl::stream, which
    does not use OpenSSL's scatter/gather interface for its primitive
    read and write operations.

    See:
        https://github.com/boostorg/beast/issues/1108
        https://github.com/boostorg/asio/issues/100
        https://stackoverflow.com/questions/38198638/openssl-ssl-write-from-multiple-buffers-ssl-writev
        https://stackoverflow.com/questions/50026167/performance-drop-on-port-from-beast-1-0-0-b66-to-boost-1-67-0-beast
*/
template<class NextLayer>
class flat_stream : private detail::flat_stream_base
{
    // Largest buffer size we will flatten
    static std::size_t constexpr max_size = 1024 * 1024;

    template<class, class> class write_op;

    NextLayer stream_;

public:
    /// The type of the next layer.
    using next_layer_type =
        typename std::remove_reference<NextLayer>::type;

    /// The type of the lowest layer.
    using lowest_layer_type = boost::beast::get_lowest_layer<next_layer_type>;

    /// The type of the executor associated with the object.
    using executor_type = typename next_layer_type::executor_type;

    ~flat_stream() = default;
    flat_stream(flat_stream&&) = default;
    flat_stream(flat_stream const&) = delete;
    flat_stream& operator=(flat_stream&&) = default;
    flat_stream& operator=(flat_stream const&) = delete;

    /** Constructor

        Arguments, if any, are forwarded to the next layer's constructor.
    */
    template<class... Args>
    explicit
    flat_stream(Args&&... args);

    //--------------------------------------------------------------------------

    /** Get the executor associated with the object.
    
        This function may be used to obtain the executor object that the
        stream uses to dispatch handlers for asynchronous operations.

        @return A copy of the executor that stream will use to dispatch handlers.
    */
    executor_type
    get_executor() noexcept
    {
        return stream_.get_executor();
    }

    /** Get a reference to the next layer

        This function returns a reference to the next layer
        in a stack of stream layers.

        @return A reference to the next layer in the stack of
        stream layers.
    */
    next_layer_type&
    next_layer()
    {
        return stream_;
    }

    /** Get a reference to the next layer

        This function returns a reference to the next layer in a
        stack of stream layers.

        @return A reference to the next layer in the stack of
        stream layers.
    */
    next_layer_type const&
    next_layer() const
    {
        return stream_;
    }

    /** Get a reference to the lowest layer

        This function returns a reference to the lowest layer
        in a stack of stream layers.

        @return A reference to the lowest layer in the stack of
        stream layers.
    */
    lowest_layer_type&
    lowest_layer()
    {
        return stream_.lowest_layer();
    }

    /** Get a reference to the lowest layer

        This function returns a reference to the lowest layer
        in a stack of stream layers.

        @return A reference to the lowest layer in the stack of
        stream layers. Ownership is not transferred to the caller.
    */
    lowest_layer_type const&
    lowest_layer() const
    {
        return stream_.lowest_layer();
    }

    //--------------------------------------------------------------------------

    template<class MutableBufferSequence>
    std::size_t
    read_some(MutableBufferSequence const& buffers);

    template<class MutableBufferSequence>
    std::size_t
    read_some(
        MutableBufferSequence const& buffers,
        boost::system::error_code& ec);

    template<class ConstBufferSequence>
    std::size_t
    write_some(ConstBufferSequence const& buffers);

    template<class ConstBufferSequence>
    std::size_t
    write_some(
        ConstBufferSequence const& buffers,
        boost::system::error_code& ec);

    template<
        class MutableBufferSequence,
        class ReadHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(
        ReadHandler, void(boost::system::error_code, std::size_t))
    async_read_some(
        MutableBufferSequence const& buffers,
        ReadHandler&& handler);

    template<
        class ConstBufferSequence,
        class WriteHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(
        WriteHandler, void(boost::system::error_code, std::size_t))
    async_write_some(
        ConstBufferSequence const& buffers,
        WriteHandler&& handler);

    template<class T>
    friend
    void
    teardown(boost::beast::websocket::role_type,
        flat_stream<T>& s, boost::system::error_code& ec);

    template<class T, class TeardownHandler>
    friend
    void
    async_teardown(boost::beast::websocket::role_type role,
        flat_stream<T>& s, TeardownHandler&& handler);
};

//------------------------------------------------------------------------------

template<class NextLayer>
template<class ConstBufferSequence, class Handler>
class flat_stream<NextLayer>::write_op
    : public boost::asio::coroutine
{
    using alloc_type = typename 
        boost::asio::associated_allocator_t<Handler>::template
            rebind<char>::other;

    struct deleter
    {
        alloc_type alloc;
        std::size_t size = 0;

        explicit
        deleter(alloc_type const& alloc_)
            : alloc(alloc_)
        {
        }

        void
        operator()(char* p)
        {
            alloc.deallocate(p, size);
        }
    };

    flat_stream<NextLayer>& s_;
    ConstBufferSequence b_;
    std::unique_ptr<char, deleter> p_;
    Handler h_;

public:
    template<class DeducedHandler>
    write_op(
        flat_stream<NextLayer>& s,
        ConstBufferSequence const& b,
        DeducedHandler&& h)
        : s_(s)
        , b_(b)
        , p_(nullptr, deleter{
            (boost::asio::get_associated_allocator)(h)})
        , h_(std::forward<DeducedHandler>(h))
    {
    }

    using allocator_type =
        boost::asio::associated_allocator_t<Handler>;

    allocator_type
    get_allocator() const noexcept
    {
        return (boost::asio::get_associated_allocator)(h_);
    }

    using executor_type = boost::asio::associated_executor_t<
        Handler, decltype(std::declval<NextLayer&>().get_executor())>;

    executor_type
    get_executor() const noexcept
    {
        return (boost::asio::get_associated_executor)(
            h_, s_.get_executor());
    }

    void
    operator()(
        boost::system::error_code ec,
        std::size_t bytes_transferred)
    {
        BOOST_ASIO_CORO_REENTER(*this)
        {
            BOOST_ASIO_CORO_YIELD
            {
                auto const result = coalesce(b_,  coalesce_limit);
                if(result.second)
                {
                    p_.get_deleter().size = result.first;
                    p_.reset(p_.get_deleter().alloc.allocate(
                        p_.get_deleter().size));
                    boost::asio::buffer_copy(
                        boost::asio::buffer(
                            p_.get(), p_.get_deleter().size),
                        b_, result.first);
                    s_.stream_.async_write_some(
                        boost::asio::buffer(
                            p_.get(), p_.get_deleter().size),
                                std::move(*this));
                }
                else
                {
                    s_.stream_.async_write_some(
                        boost::beast::buffers_prefix(result.first, b_),
                            std::move(*this));
                }
            }
            p_.reset();
            h_(ec, bytes_transferred);
        }
    }

    friend
    bool asio_handler_is_continuation(write_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
                std::addressof(op->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, write_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(op->h_));
    }
};

//------------------------------------------------------------------------------

template<class NextLayer>
template<class... Args>
flat_stream<NextLayer>::
flat_stream(Args&&... args)
    : stream_(std::forward<Args>(args)...)
{
}

template<class NextLayer>
template<class MutableBufferSequence>
std::size_t
flat_stream<NextLayer>::
read_some(MutableBufferSequence const& buffers)
{
    static_assert(boost::beast::is_sync_read_stream<next_layer_type>::value,
        "SyncReadStream requirements not met");
    static_assert(boost::asio::is_mutable_buffer_sequence<
        MutableBufferSequence>::value,
            "MutableBufferSequence requirements not met");
    boost::system::error_code ec;
    auto n = read_some(buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(boost::system::system_error{ec});
    return n;
}

template<class NextLayer>
template<class MutableBufferSequence>
std::size_t
flat_stream<NextLayer>::
read_some(MutableBufferSequence const& buffers, boost::system::error_code& ec)
{
    return stream_.read_some(buffers, ec);
}

template<class NextLayer>
template<class ConstBufferSequence>
std::size_t
flat_stream<NextLayer>::
write_some(ConstBufferSequence const& buffers)
{
    static_assert(boost::beast::is_sync_write_stream<next_layer_type>::value,
        "SyncWriteStream requirements not met");
    static_assert(boost::asio::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    boost::system::error_code ec;
    auto n = write_some(buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(boost::system::system_error{ec});
    return n;
}

template<class NextLayer>
template<class ConstBufferSequence>
std::size_t
flat_stream<NextLayer>::
write_some(
    ConstBufferSequence const& buffers,
    boost::system::error_code& ec)
{
    auto const result = coalesce(buffers, coalesce_limit);
    if(result.second)
    {
        std::unique_ptr<char[]> p{new char[result.first]};
        auto const b = boost::asio::buffer(p.get(), result.first);
        boost::asio::buffer_copy(b, buffers);
        return stream_.write_some(b, ec);
    }
    return stream_.write_some(
        boost::beast::buffers_prefix(result.first, buffers), ec);
}

template<class NextLayer>
template<
    class MutableBufferSequence,
    class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(boost::system::error_code, std::size_t))
flat_stream<NextLayer>::
async_read_some(
    MutableBufferSequence const& buffers,
    ReadHandler&& handler)
{
    static_assert(boost::beast::is_async_read_stream<next_layer_type>::value,
        "AsyncReadStream requirements not met");
    static_assert(boost::asio::is_mutable_buffer_sequence<
            MutableBufferSequence >::value,
        "MutableBufferSequence  requirements not met");
    return stream_.async_read_some(
        buffers, std::forward<ReadHandler>(handler));
}

template<class NextLayer>
template<
    class ConstBufferSequence,
    class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    WriteHandler, void(boost::system::error_code, std::size_t))
flat_stream<NextLayer>::
async_write_some(
    ConstBufferSequence const& buffers,
    WriteHandler&& handler)
{
    static_assert(boost::beast::is_async_write_stream<next_layer_type>::value,
        "AsyncWriteStream requirements not met");
    static_assert(boost::asio::is_const_buffer_sequence<
            ConstBufferSequence>::value,
        "ConstBufferSequence requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        WriteHandler, void(boost::system::error_code, std::size_t));
    write_op<ConstBufferSequence, BOOST_ASIO_HANDLER_TYPE(
        WriteHandler, void(boost::system::error_code, std::size_t))>{
            *this, buffers, std::move(init.completion_handler)}({}, 0);
    return init.result.get();
}

template<class NextLayer>
void
teardown(
    boost::beast::websocket::role_type role,
    flat_stream<NextLayer>& s,
    boost::system::error_code& ec)
{
    using boost::beast::websocket::teardown;
    teardown(role, s.stream_, ec);
}

template<class NextLayer, class TeardownHandler>
void
async_teardown(
    boost::beast::websocket::role_type role,
    flat_stream<NextLayer>& s,
    TeardownHandler&& handler)
{
    using boost::beast::websocket::async_teardown;
    async_teardown(role, s.stream_, std::move(handler));
}

#endif
