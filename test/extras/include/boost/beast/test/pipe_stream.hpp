//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_PIPE_STREAM_HPP
#define BOOST_BEAST_TEST_PIPE_STREAM_HPP

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
#include <boost/throw_exception.hpp>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <utility>

namespace boost {
namespace beast {
namespace test {

/** A bidirectional in-memory communication channel

    An instance of this class provides a client and server
    endpoint that are automatically connected to each other
    similarly to a connected socket.

    Test pipes are used to facilitate writing unit tests
    where the behavior of the transport is tightly controlled
    to help illuminate all code paths (for code coverage)
*/
class pipe
{
public:
    using buffer_type = flat_buffer;

private:
    struct read_op
    {
        virtual ~read_op() = default;
        virtual void operator()() = 0;
    };

    struct state
    {
        std::mutex m;
        buffer_type b;
        std::condition_variable cv;
        std::unique_ptr<read_op> op;
        bool eof = false;
    };

    state s_[2];

public:
    /** Represents an endpoint.

        Each pipe has a client stream and a server stream.
    */
    class stream
    {
        friend class pipe;

        template<class Handler, class Buffers>
        class read_op_impl;

        state& in_;
        state& out_;
        boost::asio::io_service& ios_;
        fail_counter* fc_ = nullptr;
        std::size_t read_max_ =
            (std::numeric_limits<std::size_t>::max)();
        std::size_t write_max_ =
            (std::numeric_limits<std::size_t>::max)();

        stream(state& in, state& out,
                boost::asio::io_service& ios)
            : in_(in)
            , out_(out)
            , ios_(ios)
            , buffer(in_.b)
        {
        }

    public:
        using buffer_type = pipe::buffer_type;

        /// Direct access to the underlying buffer
        buffer_type& buffer;

        /// Counts the number of read calls
        std::size_t nread = 0;

        /// Counts the number of write calls
        std::size_t nwrite = 0;

        ~stream() = default;
        stream(stream&&) = default;

        /// Set the fail counter on the object
#if 0
        void
        fail(fail_counter& fc)
        {
            fc_ = &fc;
        }
#endif
        /// Return the `io_service` associated with the object
        boost::asio::io_service&
        get_io_service()
        {
            return ios_;
        }

        /// Set the maximum number of bytes returned by read_some
        void
        read_size(std::size_t n)
        {
            read_max_ = n;
        }

        /// Set the maximum number of bytes returned by write_some
        void
        write_size(std::size_t n)
        {
            write_max_ = n;
        }

        /// Returns a string representing the pending input data
        string_view
        str() const
        {
            using boost::asio::buffer_cast;
            using boost::asio::buffer_size;
            return {
                buffer_cast<char const*>(*in_.b.data().begin()),
                buffer_size(*in_.b.data().begin())};
        }

        /// Clear the buffer holding the input data
        void
        clear()
        {
            in_.b.consume((std::numeric_limits<
                std::size_t>::max)());
        }

        /** Close the stream.

            The other end of the pipe will see
            `boost::asio::error::eof` on read.
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
        teardown(
            websocket::role_type role,
            stream& s, boost::system::error_code& ec);

        template<class TeardownHandler>
        friend
        void
        async_teardown(
            websocket::role_type,
            stream& s, TeardownHandler&& handler);
    };

    /** Constructor.

        The client and server endpoints will use the same `io_service`.
    */
    explicit
    pipe(boost::asio::io_service& ios)
        : client(s_[0], s_[1], ios)
        , server(s_[1], s_[0], ios)
    {
    }

    /// Represents the client endpoint
    stream client;

    /// Represents the server endpoint
    stream server;
};

//------------------------------------------------------------------------------

inline
void
teardown(
    websocket::role_type,
    pipe::stream& s,
    boost::system::error_code& ec)
{
    if(s.fc_)
    {
        if(s.fc_->fail(ec))
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
    pipe::stream& s, TeardownHandler&& handler)
{
    error_code ec;
    if(s.fc_ && s.fc_->fail(ec))
        return s.get_io_service().post(
            bind_handler(std::move(handler), ec));
    s.close();
    s.get_io_service().post(
        bind_handler(std::move(handler), ec));
}

template<class Handler, class Buffers>
class pipe::stream::read_op_impl :
    public pipe::read_op
{
    stream& s_;
    Buffers b_;
    Handler h_;

public:
    read_op_impl(stream& s,
            Buffers const& b, Handler&& h)
        : s_(s)
        , b_(b)
        , h_(std::move(h))
    {
    }

    read_op_impl(stream& s,
            Buffers const& b, Handler const& h)
        : s_(s)
        , b_(b)
        , h_(h)
    {
    }

    void
    operator()() override;
};

//------------------------------------------------------------------------------

template<class Handler, class Buffers>
void
pipe::stream::
read_op_impl<Handler, Buffers>::operator()()
{
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    s_.ios_.post(
        [&]()
        {
            BOOST_ASSERT(s_.in_.op);
            std::unique_lock<std::mutex> lock{s_.in_.m};
            if(s_.in_.b.size() > 0)
            {
                auto const bytes_transferred = buffer_copy(
                    b_, s_.in_.b.data(), s_.read_max_);
                s_.in_.b.consume(bytes_transferred);
                auto& s = s_;
                Handler h{std::move(h_)};
                s.in_.op.reset(nullptr);
                lock.unlock();
                ++s.nread;
                s.ios_.post(bind_handler(std::move(h),
                    error_code{}, bytes_transferred));
            }
            else
            {
                BOOST_ASSERT(s_.in_.eof);
                auto& s = s_;
                Handler h{std::move(h_)};
                s.in_.op.reset(nullptr);
                lock.unlock();
                ++s.nread;
                s.ios_.post(bind_handler(std::move(h),
                    boost::asio::error::eof, 0));
            }
        });
}

//------------------------------------------------------------------------------

template<class>
void
pipe::stream::
close()
{
    std::lock_guard<std::mutex> lock{out_.m};
    if(! out_.eof)
    {
        out_.eof = true;
        if(out_.op)
            out_.op.get()->operator()();
        else
            out_.cv.notify_all();
    }
}


template<class MutableBufferSequence>
std::size_t
pipe::stream::
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
pipe::stream::
read_some(MutableBufferSequence const& buffers,
    error_code& ec)
{
    static_assert(is_mutable_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence requirements not met");
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    BOOST_ASSERT(! in_.op);
    BOOST_ASSERT(buffer_size(buffers) > 0);
    if(fc_ && fc_->fail(ec))
        return 0;
    std::unique_lock<std::mutex> lock{in_.m};
    in_.cv.wait(lock,
        [&]()
        {
            return in_.b.size() > 0 || in_.eof;
        });
    std::size_t bytes_transferred;
    if(in_.b.size() > 0)
    {   
        ec.assign(0, ec.category());
        bytes_transferred = buffer_copy(
            buffers, in_.b.data(), read_max_);
        in_.b.consume(bytes_transferred);
    }
    else
    {
        BOOST_ASSERT(in_.eof);
        bytes_transferred = 0;
        ec = boost::asio::error::eof;
    }
    ++nread;
    return bytes_transferred;
}

template<class MutableBufferSequence, class ReadHandler>
async_return_type<
    ReadHandler, void(error_code, std::size_t)>
pipe::stream::
async_read_some(MutableBufferSequence const& buffers,
    ReadHandler&& handler)
{
    static_assert(is_mutable_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence requirements not met");
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    BOOST_ASSERT(! in_.op);
    BOOST_ASSERT(buffer_size(buffers) > 0);
    async_completion<ReadHandler,
        void(error_code, std::size_t)> init{handler};
    if(fc_)
    {
        error_code ec;
        if(fc_->fail(ec))
            return ios_.post(bind_handler(
                init.completion_handler, ec, 0));
    }
    {
        std::unique_lock<std::mutex> lock{in_.m};
        if(in_.eof)
        {
            lock.unlock();
            ++nread;
            ios_.post(bind_handler(init.completion_handler,
                boost::asio::error::eof, 0));
        }
        else if(buffer_size(buffers) == 0 ||
            buffer_size(in_.b.data()) > 0)
        {
            auto const bytes_transferred = buffer_copy(
                buffers, in_.b.data(), read_max_);
            in_.b.consume(bytes_transferred);
            lock.unlock();
            ++nread;
            ios_.post(bind_handler(init.completion_handler,
                error_code{}, bytes_transferred));
        }
        else
        {
            in_.op.reset(new read_op_impl<handler_type<
                ReadHandler, void(error_code, std::size_t)>,
                    MutableBufferSequence>{*this, buffers,
                        init.completion_handler});
        }
    }
    return init.result.get();
}

template<class ConstBufferSequence>
std::size_t
pipe::stream::
write_some(ConstBufferSequence const& buffers)
{
    static_assert(is_const_buffer_sequence<
            ConstBufferSequence>::value,
        "ConstBufferSequence requirements not met");
    BOOST_ASSERT(! out_.eof);
    error_code ec;
    auto const bytes_transferred =
        write_some(buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<class ConstBufferSequence>
std::size_t
pipe::stream::
write_some(
    ConstBufferSequence const& buffers, error_code& ec)
{
    static_assert(is_const_buffer_sequence<
            ConstBufferSequence>::value,
        "ConstBufferSequence requirements not met");
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    BOOST_ASSERT(! out_.eof);
    if(fc_ && fc_->fail(ec))
        return 0;
    auto const n = (std::min)(
        buffer_size(buffers), write_max_);
    std::unique_lock<std::mutex> lock{out_.m};
    auto const bytes_transferred =
        buffer_copy(out_.b.prepare(n), buffers);
    out_.b.commit(bytes_transferred);
    if(out_.op)
        out_.op.get()->operator()();
    else
        out_.cv.notify_all();
    lock.unlock();
    ++nwrite;
    ec.assign(0, ec.category());
    return bytes_transferred;
}

template<class ConstBufferSequence, class WriteHandler>
async_return_type<
    WriteHandler, void(error_code, std::size_t)>
pipe::stream::
async_write_some(ConstBufferSequence const& buffers,
    WriteHandler&& handler)
{
    static_assert(is_const_buffer_sequence<
            ConstBufferSequence>::value,
        "ConstBufferSequence requirements not met");
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    BOOST_ASSERT(! out_.eof);
    async_completion<WriteHandler,
        void(error_code, std::size_t)> init{handler};
    if(fc_)
    {
        error_code ec;
        if(fc_->fail(ec))
            return ios_.post(bind_handler(
                init.completion_handler, ec, 0));
    }
    auto const n =
        (std::min)(buffer_size(buffers), write_max_);
    std::unique_lock<std::mutex> lock{out_.m};
    auto const bytes_transferred =
        buffer_copy(out_.b.prepare(n), buffers);
    out_.b.commit(bytes_transferred);
    if(out_.op)
        out_.op.get()->operator()();
    else
        out_.cv.notify_all();
    lock.unlock();
    ++nwrite;
    ios_.post(bind_handler(init.completion_handler,
        error_code{}, bytes_transferred));
    return init.result.get();
}

} // test
} // beast
} // boost

#endif
