//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_STREAM_HPP
#define BOOST_BEAST_TEST_STREAM_HPP

#include <boost/beast/core/async_result.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/websocket/teardown.hpp>
#include <boost/beast/test/fail_counter.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/assert.hpp>
#include <boost/optional.hpp>
#include <boost/throw_exception.hpp>
#include <condition_variable>
#include <limits>
#include <memory>
#include <mutex>
#include <utility>

namespace boost {
namespace beast {
namespace test {

class stream;

namespace detail {

class stream_impl
{
    friend class boost::beast::test::stream;

    using buffer_type = flat_buffer;

    struct read_op
    {
        virtual ~read_op() = default;
        virtual void operator()() = 0;
    };

    template<class Handler, class Buffers>
    class read_op_impl;

    enum class status
    {
        ok,
        eof,
        reset
    };

    struct state
    {
        friend class stream;

        std::mutex m;
        buffer_type b;
        std::condition_variable cv;
        std::unique_ptr<read_op> op;
        boost::asio::io_service& ios;
        status code = status::ok;
        fail_counter* fc = nullptr;
        std::size_t nread = 0;
        std::size_t nwrite = 0;
        std::size_t read_max =
            (std::numeric_limits<std::size_t>::max)();
        std::size_t write_max =
            (std::numeric_limits<std::size_t>::max)();

        explicit
        state(
            boost::asio::io_service& ios_,
            fail_counter* fc_)
            : ios(ios_)
            , fc(fc_)
        {
        }

        ~state()
        {
            BOOST_ASSERT(! op);
        }

        void
        on_write()
        {
            if(op)
            {
                std::unique_ptr<read_op> op_ = std::move(op);
                op_->operator()();
            }
            else
            {
                cv.notify_all();
            }
        }
    };

    state s0_;
    state s1_;

public:
    stream_impl(
        boost::asio::io_service& ios,
        fail_counter* fc)
        : s0_(ios, fc)
        , s1_(ios, nullptr)
    {
    }

    stream_impl(
        boost::asio::io_service& ios0,
        boost::asio::io_service& ios1)
        : s0_(ios0, nullptr)
        , s1_(ios1, nullptr)
    {
    }

    ~stream_impl()
    {
        BOOST_ASSERT(! s0_.op);
        BOOST_ASSERT(! s1_.op);
    }
};

template<class Handler, class Buffers>
class stream_impl::read_op_impl : public stream_impl::read_op
{
    class lambda
    {
        state& s_;
        Buffers b_;
        Handler h_;
        boost::optional<
            boost::asio::io_service::work> work_;

    public:
        lambda(lambda&&) = default;
        lambda(lambda const&) = default;

        lambda(state& s, Buffers const& b, Handler&& h)
            : s_(s)
            , b_(b)
            , h_(std::move(h))
            , work_(s_.ios)
        {
        }

        lambda(state& s, Buffers const& b, Handler const& h)
            : s_(s)
            , b_(b)
            , h_(h)
            , work_(s_.ios)
        {
        }

        void
        post()
        {
            s_.ios.post(std::move(*this));
            work_ = boost::none;
        }

        void
        operator()()
        {
            using boost::asio::buffer_copy;
            using boost::asio::buffer_size;
            std::unique_lock<std::mutex> lock{s_.m};
            BOOST_ASSERT(! s_.op);
            if(s_.b.size() > 0)
            {
                auto const bytes_transferred = buffer_copy(
                    b_, s_.b.data(), s_.read_max);
                s_.b.consume(bytes_transferred);
                auto& s = s_;
                Handler h{std::move(h_)};
                lock.unlock();
                ++s.nread;
                s.ios.post(bind_handler(std::move(h),
                    error_code{}, bytes_transferred));
            }
            else
            {
                BOOST_ASSERT(s_.code != status::ok);
                auto& s = s_;
                Handler h{std::move(h_)};
                lock.unlock();
                ++s.nread;
                error_code ec;
                if(s.code == status::eof)
                    ec = boost::asio::error::eof;
                else if(s.code == status::reset)
                    ec = boost::asio::error::connection_reset;
                s.ios.post(bind_handler(std::move(h), ec, 0));
            }
        }
    };

    lambda fn_;

public:
    read_op_impl(state& s, Buffers const& b, Handler&& h)
        : fn_(s, b, std::move(h))
    {
    }

    read_op_impl(state& s, Buffers const& b, Handler const& h)
        : fn_(s, b, h)
    {
    }

    void
    operator()() override
    {
        fn_.post();
    }
};

} // detail

//------------------------------------------------------------------------------

/** A bidirectional in-memory communication channel

    An instance of this class provides a client and server
    endpoint that are automatically connected to each other
    similarly to a connected socket.

    Test pipes are used to facilitate writing unit tests
    where the behavior of the transport is tightly controlled
    to help illuminate all code paths (for code coverage)
*/
class stream
{
    using status = detail::stream_impl::status;

    std::shared_ptr<detail::stream_impl> impl_;
    detail::stream_impl::state* in_;
    detail::stream_impl::state* out_;

    explicit
    stream(std::shared_ptr<
            detail::stream_impl> const& impl)
        : impl_(impl)
        , in_(&impl_->s1_)
        , out_(&impl_->s0_)
    {
    }

public:
    using buffer_type = flat_buffer;

    /// Assignment
    stream& operator=(stream&&) = default;

    /// Destructor
    ~stream()
    {
        if(! impl_)
            return;
        std::unique_lock<std::mutex> lock{out_->m};
        if(out_->code == status::ok)
        {
            out_->code = status::reset;
            out_->on_write();
        }
        lock.unlock();
    }

    /// Constructor
    stream(stream&& other)
        : impl_(std::move(other.impl_))
        , in_(other.in_)
        , out_(other.out_)
    {
    }

    /// Constructor
    explicit
    stream(
        boost::asio::io_service& ios)
        : impl_(std::make_shared<
            detail::stream_impl>(ios, nullptr))
        , in_(&impl_->s0_)
        , out_(&impl_->s1_)
    {
    }

    /// Constructor
    explicit
    stream(
        boost::asio::io_service& ios0,
        boost::asio::io_service& ios1)
        : impl_(std::make_shared<
            detail::stream_impl>(ios0, ios1))
        , in_(&impl_->s0_)
        , out_(&impl_->s1_)
    {
    }

    /// Constructor
    explicit
    stream(
        boost::asio::io_service& ios,
        fail_counter& fc)
        : impl_(std::make_shared<
            detail::stream_impl>(ios, &fc))
        , in_(&impl_->s0_)
        , out_(&impl_->s1_)
    {
    }

    /// Constructor
    stream(
        boost::asio::io_service& ios,
        string_view s)
        : impl_(std::make_shared<
            detail::stream_impl>(ios, nullptr))
        , in_(&impl_->s0_)
        , out_(&impl_->s1_)
    {
        using boost::asio::buffer;
        using boost::asio::buffer_copy;
        in_->b.commit(buffer_copy(
            in_->b.prepare(s.size()),
            buffer(s.data(), s.size())));
    }

    /// Constructor
    stream(
        boost::asio::io_service& ios,
        fail_counter& fc,
        string_view s)
        : impl_(std::make_shared<
            detail::stream_impl>(ios, &fc))
        , in_(&impl_->s0_)
        , out_(&impl_->s1_)
    {
        using boost::asio::buffer;
        using boost::asio::buffer_copy;
        in_->b.commit(buffer_copy(
            in_->b.prepare(s.size()),
            buffer(s.data(), s.size())));
    }

    /// Return the other end of the connection
    stream
    remote()
    {
        BOOST_ASSERT(in_ == &impl_->s0_);
        return stream{impl_};
    }

    /// Return the `io_service` associated with the object
    boost::asio::io_service&
    get_io_service()
    {
        return in_->ios;
    }

    /// Set the maximum number of bytes returned by read_some
    void
    read_size(std::size_t n)
    {
        in_->read_max = n;
    }

    /// Set the maximum number of bytes returned by write_some
    void
    write_size(std::size_t n)
    {
        out_->write_max = n;
    }

    /// Direct input buffer access
    buffer_type&
    buffer()
    {
        return in_->b;
    }

    /// Returns a string view representing the pending input data
    string_view
    str() const
    {
        using boost::asio::buffer_cast;
        using boost::asio::buffer_size;
        return {
            buffer_cast<char const*>(*in_->b.data().begin()),
            buffer_size(*in_->b.data().begin())};
    }

    /// Appends a string to the pending input data
    void
    str(string_view s)
    {
        using boost::asio::buffer;
        using boost::asio::buffer_copy;
        std::lock_guard<std::mutex> lock{in_->m};
        in_->b.commit(buffer_copy(
            in_->b.prepare(s.size()),
            buffer(s.data(), s.size())));
    }

    /// Clear the pending input area
    void
    clear()
    {
        std::lock_guard<std::mutex> lock{in_->m};
        in_->b.consume(in_->b.size());
    }

    /// Return the number of reads
    std::size_t
    nread() const
    {
        return in_->nread;
    }

    /// Return the number of writes
    std::size_t
    nwrite() const
    {
        return out_->nwrite;
    }

    /** Close the stream.

        The other end of the pipe will see `error::eof`
        after reading all the data from the buffer.
    */
    template<class = void>
    void
    close();

    template<class MutableBufferSequence>
    std::size_t
    read_some(MutableBufferSequence const& buffers);

    template<class MutableBufferSequence>
    std::size_t
    read_some(MutableBufferSequence const& buffers,
        error_code& ec);

    template<class MutableBufferSequence, class ReadHandler>
    async_return_type<
        ReadHandler, void(error_code, std::size_t)>
    async_read_some(MutableBufferSequence const& buffers,
        ReadHandler&& handler);

    template<class ConstBufferSequence>
    std::size_t
    write_some(ConstBufferSequence const& buffers);

    template<class ConstBufferSequence>
    std::size_t
    write_some(
        ConstBufferSequence const& buffers, error_code&);

    template<class ConstBufferSequence, class WriteHandler>
    async_return_type<
        WriteHandler, void(error_code, std::size_t)>
    async_write_some(ConstBufferSequence const& buffers,
        WriteHandler&& handler);

    friend
    void
    teardown(websocket::role_type,
        stream& s, boost::system::error_code& ec);

    template<class TeardownHandler>
    friend
    void
    async_teardown(websocket::role_type role,
        stream& s, TeardownHandler&& handler);
};

//------------------------------------------------------------------------------

template<class MutableBufferSequence>
std::size_t
stream::
read_some(MutableBufferSequence const& buffers)
{
    static_assert(is_mutable_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence requirements not met");
    error_code ec;
    auto const n = read_some(buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return n;
}

template<class MutableBufferSequence>
std::size_t
stream::
read_some(MutableBufferSequence const& buffers,
    error_code& ec)
{
    static_assert(is_mutable_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence requirements not met");
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    BOOST_ASSERT(buffer_size(buffers) > 0);
    if(in_->fc && in_->fc->fail(ec))
        return 0;
    std::unique_lock<std::mutex> lock{in_->m};
    BOOST_ASSERT(! in_->op);
    in_->cv.wait(lock,
        [&]()
        {
            return
                in_->b.size() > 0 ||
                in_->code != status::ok;
        });
    std::size_t bytes_transferred;
    if(in_->b.size() > 0)
    {   
        ec.assign(0, ec.category());
        bytes_transferred = buffer_copy(
            buffers, in_->b.data(), in_->read_max);
        in_->b.consume(bytes_transferred);
    }
    else
    {
        BOOST_ASSERT(in_->code != status::ok);
        bytes_transferred = 0;
        if(in_->code == status::eof)
            ec = boost::asio::error::eof;
        else if(in_->code == status::reset)
            ec = boost::asio::error::connection_reset;
    }
    ++in_->nread;
    return bytes_transferred;
}

template<class MutableBufferSequence, class ReadHandler>
async_return_type<
    ReadHandler, void(error_code, std::size_t)>
stream::
async_read_some(
    MutableBufferSequence const& buffers,
    ReadHandler&& handler)
{
    static_assert(is_mutable_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence requirements not met");
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    BOOST_ASSERT(buffer_size(buffers) > 0);
    async_completion<ReadHandler,
        void(error_code, std::size_t)> init{handler};
    if(in_->fc)
    {
        error_code ec;
        if(in_->fc->fail(ec))
            return in_->ios.post(bind_handler(
                init.completion_handler, ec, 0));
    }
    {
        std::unique_lock<std::mutex> lock{in_->m};
        BOOST_ASSERT(! in_->op);
        if(buffer_size(buffers) == 0 ||
            buffer_size(in_->b.data()) > 0)
        {
            auto const bytes_transferred = buffer_copy(
                buffers, in_->b.data(), in_->read_max);
            in_->b.consume(bytes_transferred);
            lock.unlock();
            ++in_->nread;
            in_->ios.post(bind_handler(init.completion_handler,
                error_code{}, bytes_transferred));
        }
        else if(in_->code != status::ok)
        {
            lock.unlock();
            ++in_->nread;
            error_code ec;
            if(in_->code == status::eof)
                ec = boost::asio::error::eof;
            else if(in_->code == status::reset)
                ec = boost::asio::error::connection_reset;
            in_->ios.post(bind_handler(
                init.completion_handler, ec, 0));
        }
        else
        {
            in_->op.reset(new
                detail::stream_impl::read_op_impl<handler_type<
                    ReadHandler, void(error_code, std::size_t)>,
                        MutableBufferSequence>{*in_, buffers,
                            init.completion_handler});
        }
    }
    return init.result.get();
}

template<class ConstBufferSequence>
std::size_t
stream::
write_some(ConstBufferSequence const& buffers)
{
    static_assert(is_const_buffer_sequence<
            ConstBufferSequence>::value,
        "ConstBufferSequence requirements not met");
    BOOST_ASSERT(out_->code == status::ok);
    error_code ec;
    auto const bytes_transferred =
        write_some(buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<class ConstBufferSequence>
std::size_t
stream::
write_some(
    ConstBufferSequence const& buffers, error_code& ec)
{
    static_assert(is_const_buffer_sequence<
            ConstBufferSequence>::value,
        "ConstBufferSequence requirements not met");
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    BOOST_ASSERT(out_->code == status::ok);
    if(in_->fc && in_->fc->fail(ec))
        return 0;
    auto const n = (std::min)(
        buffer_size(buffers), out_->write_max);
    std::unique_lock<std::mutex> lock{out_->m};
    auto const bytes_transferred =
        buffer_copy(out_->b.prepare(n), buffers);
    out_->b.commit(bytes_transferred);
    out_->on_write();
    lock.unlock();
    ++out_->nwrite;
    ec.assign(0, ec.category());
    return bytes_transferred;
}

template<class ConstBufferSequence, class WriteHandler>
async_return_type<
    WriteHandler, void(error_code, std::size_t)>
stream::
async_write_some(ConstBufferSequence const& buffers,
    WriteHandler&& handler)
{
    static_assert(is_const_buffer_sequence<
            ConstBufferSequence>::value,
        "ConstBufferSequence requirements not met");
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    BOOST_ASSERT(out_->code == status::ok);
    async_completion<WriteHandler,
        void(error_code, std::size_t)> init{handler};
    if(in_->fc)
    {
        error_code ec;
        if(in_->fc->fail(ec))
            return in_->ios.post(bind_handler(
                init.completion_handler, ec, 0));
    }
    auto const n =
        (std::min)(buffer_size(buffers), out_->write_max);
    std::unique_lock<std::mutex> lock{out_->m};
    auto const bytes_transferred =
        buffer_copy(out_->b.prepare(n), buffers);
    out_->b.commit(bytes_transferred);
    out_->on_write();
    lock.unlock();
    ++out_->nwrite;
    in_->ios.post(bind_handler(init.completion_handler,
        error_code{}, bytes_transferred));
    return init.result.get();
}

inline
void
teardown(websocket::role_type,
    stream& s, boost::system::error_code& ec)
{
    if(s.in_->fc)
    {
        if(s.in_->fc->fail(ec))
            return;
    }
    else
    {
        s.close();
        ec.assign(0, ec.category());
    }
}

template<class TeardownHandler>
inline
void
async_teardown(websocket::role_type,
    stream& s, TeardownHandler&& handler)
{
    error_code ec;
    if(s.in_->fc && s.in_->fc->fail(ec))
        return s.get_io_service().post(
            bind_handler(std::move(handler), ec));
    s.close();
    s.get_io_service().post(
        bind_handler(std::move(handler), ec));
}

template<class>
void
stream::
close()
{
    BOOST_ASSERT(! in_->op);
    std::lock_guard<std::mutex> lock{out_->m};
    if(out_->code == status::ok)
    {
        out_->code = status::eof;
        out_->on_write();
    }
}

} // test
} // beast
} // boost

#endif
