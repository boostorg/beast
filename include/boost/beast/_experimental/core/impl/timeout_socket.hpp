//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_IMPL_TIMEOUT_SOCKET_HPP
#define BOOST_BEAST_CORE_IMPL_TIMEOUT_SOCKET_HPP

#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/_experimental/core/timeout_work_guard.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <memory>
#include <utility>

namespace boost {
namespace beast {

template<class Protocol, class Executor>
template<class Handler>
class basic_timeout_socket<Protocol, Executor>::async_op
{
public:
    async_op(async_op&&) = default;
    async_op(async_op const&) = delete;

    template<class Buffers, class DeducedHandler>
    async_op(
        Buffers const& b,
        DeducedHandler&& h,
        basic_timeout_socket& s,
        std::true_type)
        : h_(std::forward<DeducedHandler>(h))
        , s_(s)
        , work_(s.rd_timer_)
        , wg0_(s_.get_executor())
        , wg1_(get_executor())
        , saved_(s_.rd_op_)

    {
        s_.sock_.async_read_some(b, std::move(*this));
    }

    template<class Buffers, class DeducedHandler>
    async_op(
        Buffers const& b,
        DeducedHandler&& h,
        basic_timeout_socket& s,
        std::false_type)
        : h_(std::forward<DeducedHandler>(h))
        , s_(s)
        , work_(s.wr_timer_)
        , wg0_(s_.get_executor())
        , wg1_(get_executor())
        , saved_(s_.wr_op_)
    {
        s_.sock_.async_write_some(b, std::move(*this));
    }

    using allocator_type =
        boost::asio::associated_allocator_t<Handler>;

    allocator_type
    get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(h_);
    }

    using executor_type =
        boost::asio::associated_executor_t<Handler,
            decltype(std::declval<basic_timeout_socket<
                Protocol, Executor>&>().get_executor())>;

    executor_type
    get_executor() const noexcept
    {
        return boost::asio::get_associated_executor(
            h_, s_.get_executor());
    }

    friend
    bool asio_handler_is_continuation(async_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, async_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(op->h_));
    }

    void
    operator()(error_code ec, std::size_t bytes_transferred)
    {
        if(! work_.try_complete())
        {
            saved_.emplace(
                std::move(*this),
                ec,
                bytes_transferred);
            return;
        }
        h_(ec, bytes_transferred);
    }

private:
    Handler h_;
    basic_timeout_socket& s_;
    timeout_work_guard work_;
    boost::asio::executor_work_guard<Executor> wg0_;
    boost::asio::executor_work_guard<executor_type> wg1_;
    detail::saved_handler& saved_;
};

//------------------------------------------------------------------------------

template<class Protocol, class Executor>
template<class ExecutionContext, class>
basic_timeout_socket<Protocol, Executor>::
basic_timeout_socket(ExecutionContext& ctx)
    : ex_(ctx.get_executor())
    , rd_timer_(ctx)
    , wr_timer_(ctx)
    , cn_timer_(ctx)
    , sock_(ctx)
{
    rd_timer_.set_callback(ex_,
        [this]
        {
            if(rd_op_.empty())
                sock_.cancel();
            else
                rd_op_();
        });

    wr_timer_.set_callback(ex_,
        [this]
        {
            if(wr_op_.empty())
                sock_.cancel();
            else
                wr_op_();
        });

    cn_timer_.set_callback(ex_,
        [this]
        {
            if(cn_op_.empty())
                sock_.cancel();
            else
                cn_op_();
        });
}

template<class Protocol, class Executor>
basic_timeout_socket<Protocol, Executor>::
~basic_timeout_socket()
{
    rd_timer_.destroy();
    wr_timer_.destroy();
}

template<class Protocol, class Executor>
template<class MutableBufferSequence, class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(ReadHandler,
    void(boost::system::error_code, std::size_t))
basic_timeout_socket<Protocol, Executor>::
async_read_some(
    MutableBufferSequence const& buffers,
    ReadHandler&& handler)
{
    static_assert(boost::asio::is_mutable_buffer_sequence<
        MutableBufferSequence>::value,
            "MutableBufferSequence requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        ReadHandler, void(error_code, std::size_t));
    async_op<BOOST_ASIO_HANDLER_TYPE(ReadHandler,
        void(error_code, std::size_t))>(buffers,
            std::forward<ReadHandler>(handler), *this,
                std::true_type{});
    return init.result.get();
}

template<class Protocol, class Executor>
template<class ConstBufferSequence, class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(WriteHandler,
    void(boost::system::error_code, std::size_t))
basic_timeout_socket<Protocol, Executor>::
async_write_some(
    ConstBufferSequence const& buffers,
    WriteHandler&& handler)
{
    static_assert(boost::asio::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        WriteHandler, void(error_code, std::size_t));
    async_op<BOOST_ASIO_HANDLER_TYPE(WriteHandler,
        void(error_code, std::size_t))>(buffers,
            std::forward<WriteHandler>(handler), *this,
                std::false_type{});
    return init.result.get();
}

//------------------------------------------------------------------------------

namespace detail {

template<
    class Protocol, class Executor,
    class Handler>
class connect_op
{
public:
    template<
        class Endpoints,
        class Condition,
        class DeducedHandler>
    connect_op(
        basic_timeout_socket<Protocol, Executor>& s,
        Endpoints const& eps,
        Condition cond,
        DeducedHandler&& h)
        : h_(std::forward<DeducedHandler>(h))
        , work_(s.cnd_timer_)
        , s_(s)
        , wg0_(s_.get_executor())
        , wg1_(get_executor())
    {
        boost::asio::async_connect(
            s_.next_layer(), eps, cond,
                std::move(*this));
    }

    using allocator_type =
        boost::asio::associated_allocator_t<Handler>;

    allocator_type
    get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(h_);
    }

    using executor_type =
        boost::asio::associated_executor_t<Handler,
            decltype(std::declval<basic_timeout_socket<
                Protocol, Executor>&>().get_executor())>;

    executor_type
    get_executor() const noexcept
    {
        return boost::asio::get_associated_executor(
            h_, s_.get_executor());
    }

    friend
    bool asio_handler_is_continuation(connect_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, connect_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(op->h_));
    }

    template<class Arg>
    void
    operator()(error_code ec, Arg&& arg)
    {
        if(! work_.try_complete())
        {
            s_.cn_op_.emplace(
                std::move(*this),
                ec,
                std::forward<Arg>(arg));
            return;
        }
        h_(ec, std::forward<Arg>(arg));
    }

private:
    Handler h_;
    timeout_work_guard work_;
    basic_timeout_socket<Protocol, Executor>& s_;
    boost::asio::executor_work_guard<Executor> wg0_;
    boost::asio::executor_work_guard<executor_type> wg1_;
};

struct any_endpoint
{
    template<class Error, class Endpoint>
    bool
    operator()(Error const&, Endpoint const&) const noexcept
    {
        return true;
    }
};

template<class Iterator>
struct endpoint_range_type
{
    using const_iterator = Iterator;

    Iterator begin_;
    Iterator end_;

    Iterator begin() const noexcept
    {
        return begin_;
    }

    Iterator end() const noexcept
    {
        return end_;
    }
};

template<class Iterator>
endpoint_range_type<Iterator>
endpoint_range(Iterator first, Iterator last)
{
    return endpoint_range_type<
        Iterator>(first, last);
}

} // detail

template<
    class Protocol, class Executor,
    class EndpointSequence,
    class RangeConnectHandler, class>
BOOST_ASIO_INITFN_RESULT_TYPE(RangeConnectHandler,
    void(boost::system::error_code, typename Protocol::endpoint))
async_connect(
    basic_timeout_socket<Protocol, Executor>& s,
    EndpointSequence const& endpoints,
    RangeConnectHandler&& handler)
{
    BOOST_BEAST_HANDLER_INIT(RangeConnectHandler,
        void(boost::system::error_code, typename Protocol::endpoint));
    detail::connect_op<Protocol, Executor,
        BOOST_ASIO_HANDLER_TYPE(RangeConnectHandler,
            void(boost::system::error_code,
                typename Protocol::endpoint))>(
        s, endpoints, detail::any_endpoint{},
            std::forward<RangeConnectHandler>(handler));
    return init.result.get();
}

template<
    class Protocol, class Executor,
    class EndpointSequence,
    class ConnectCondition,
    class RangeConnectHandler, class>
BOOST_ASIO_INITFN_RESULT_TYPE(RangeConnectHandler,
    void (boost::system::error_code, typename Protocol::endpoint))
async_connect(
    basic_timeout_socket<Protocol, Executor>& s,
    EndpointSequence const& endpoints,
    ConnectCondition connect_condition,
    RangeConnectHandler&& handler)
{
    BOOST_BEAST_HANDLER_INIT(RangeConnectHandler,
        void(boost::system::error_code, typename Protocol::endpoint));
    detail::connect_op<Protocol, Executor,
        BOOST_ASIO_HANDLER_TYPE(RangeConnectHandler,
            void(boost::system::error_code,
                typename Protocol::endpoint))>(
        s, endpoints, connect_condition,
            std::forward<RangeConnectHandler>(handler));
    return init.result.get();
}

template<
    class Protocol, class Executor,
    class Iterator,
    class IteratorConnectHandler, class>
BOOST_ASIO_INITFN_RESULT_TYPE(IteratorConnectHandler,
    void (boost::system::error_code, Iterator))
async_connect(
    basic_timeout_socket<Protocol, Executor>& s,
    Iterator begin, Iterator end,
    IteratorConnectHandler&& handler)
{
    BOOST_BEAST_HANDLER_INIT(IteratorConnectHandler,
        void(boost::system::error_code, Iterator));
    detail::connect_op<Protocol, Executor,
        BOOST_ASIO_HANDLER_TYPE(IteratorConnectHandler,
            void(boost::system::error_code, Iterator))>(
        s, detail::endpoint_range(begin, end), detail::any_endpoint{},
            std::forward<IteratorConnectHandler>(handler));
    return init.result.get();
}

template<
    class Protocol, class Executor,
    class Iterator,
    class ConnectCondition,
    class IteratorConnectHandler, class>
BOOST_ASIO_INITFN_RESULT_TYPE(IteratorConnectHandler,
    void (boost::system::error_code, Iterator))
async_connect(
    basic_timeout_socket<Protocol, Executor>& s,
    Iterator begin, Iterator end,
    ConnectCondition connect_condition,
    IteratorConnectHandler&& handler)
{
    BOOST_BEAST_HANDLER_INIT(IteratorConnectHandler,
        void(boost::system::error_code, Iterator));
    detail::connect_op<Protocol, Executor,
        BOOST_ASIO_HANDLER_TYPE(IteratorConnectHandler,
            void(boost::system::error_code, Iterator))>(
        s, detail::endpoint_range(begin, end), connect_condition,
            std::forward<IteratorConnectHandler>(handler));
    return init.result.get();
}

} // beast
} // boost

#endif
