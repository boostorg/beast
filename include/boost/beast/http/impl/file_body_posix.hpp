// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_BEAST_HTTP_IMPL_FILE_BODY_POSIX_HPP
#define BOOST_BEAST_HTTP_IMPL_FILE_BODY_POSIX_HPP

#include <boost/beast/core/file_posix.hpp>

#if BOOST_BEAST_USE_POSIX_FILE

#if defined(__linux__)
#define BOOST_BEAST_HAS_SENDFILE 1
#define BOOST_BEAST_SENDFILE_POSIX_STYLE 1
#elif defined(__FreeBSD__) || defined(__FreeBSD)
#define BOOST_BEAST_HAS_SENDFILE 1
#define BOOST_BEAST_SENDFILE_FREEBSD_STYLE 1
#elif defined(__APPLE__)
#define BOOST_BEAST_HAS_SENDFILE 1
#define BOOST_BEAST_SENDFILE_APPLE_STYLE 1
#endif

#if BOOST_BEAST_HAS_SENDFILE
#include <boost/beast/http/write.hpp>
#include <boost/beast/http/basic_file_body.hpp>
#include <boost/beast/http/serializer.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/beast/core/buffers_range.hpp>

#if defined(__linux__)
#include <sys/sendfile.h>
#else
#include <sys/uio.h>
#endif

namespace boost {
namespace beast {
namespace http {
namespace detail {

template<class, class, bool, class, class>
class write_some_posix_op;

#if BOOST_BEAST_SENDFILE_POSIX_STYLE

inline ssize_t call_sendfile(int out_fd, int in_fd, off_t &offset, size_t count)
{
    return ::sendfile(out_fd, in_fd, &offset, count);
}

#elif BOOST_BEAST_SENDFILE_FREEBSD_STYLE

inline ssize_t call_sendfile(int out_fd, int in_fd, off_t &offset, size_t count)
{
    off_t read;
    const auto res = ::sendfile(in_fd, out_fd, offset, count, nullptr, &read, 0);
    if (res != -1)
    {
        offset += read;
        return static_cast<ssize_t>(read);
    }
    else
        return static_cast<ssize_t>(-1);
}

#elif BOOST_BEAST_SENDFILE_APPLE_STYLE

inline ssize_t call_sendfile(int out_fd, int in_fd, off_t &offset, size_t count)
{
    off_t read = count;
    const auto res = ::sendfile(in_fd, out_fd, offset, &read, nullptr, 0);
    if (res != -1)
    {
        offset += read;
        return static_cast<ssize_t>(read);
    }
    else
        return static_cast<ssize_t>(-1);
}

#endif


class null_lambda
{
  public:
    template<class ConstBufferSequence>
    void
    operator()(error_code&,
               ConstBufferSequence const&) const
    {
        BOOST_ASSERT(false);
    }
};

inline error_code make_sendfile_error(int error)
{
    switch (errno)
    {
        case ESPIPE:    return net::error::invalid_argument;
        case EBADF:     return net::error::bad_descriptor;
        case EFAULT:    return net::error::fault;
        case EINVAL:    return net::error::invalid_argument;
        case EIO:       return net::error::fault;
        case ENOMEM:    return net::error::no_memory;
        case EOVERFLOW: return net::error::message_size;
        default:
            return error_code (errno, system::system_category());
    }
}

}


template<>
struct basic_file_body<file_posix>
{
    using file_type = file_posix;

    class writer;
    class reader;

    //--------------------------------------------------------------------------

    class value_type
    {
        friend class writer;
        friend class reader;
        friend struct basic_file_body<file_posix>;

        template<class, class, bool, class, class>
        friend class detail::write_some_posix_op;

        template<class, class, bool, class, class>
        friend class detail::write_some_posix_op;
        template<
                class Protocol, class Executor,
                bool isRequest, class Fields>
        friend
        std::size_t
        write_some(
                net::basic_stream_socket<Protocol, Executor>& sock,
                serializer<isRequest,
                basic_file_body<file_posix>, Fields>& sr,
                error_code& ec);

        file_posix file_;
        std::uint64_t size_ = 0;    // cached file size
        std::uint64_t first_ = 0;   // starting offset of the range
        std::uint64_t last_;        // ending offset of the range

      public:
        ~value_type() = default;
        value_type() = default;
        value_type(value_type&& other) = default;
        value_type& operator=(value_type&& other) = default;

        file_posix& file()
        {
            return file_;
        }

        bool
        is_open() const
        {
            return file_.is_open();
        }

        std::uint64_t
        size() const
        {
            return size_;
        }

        void
        close();

        void
        open(char const* path, file_mode mode, error_code& ec);

        void
        reset(file_posix&& file, error_code& ec);
    };

    //--------------------------------------------------------------------------

    class writer
    {
        template<class, class, bool, class, class>
        friend class detail::write_some_posix_op;
        template<
                class Protocol, class Executor,
                bool isRequest, class Fields>
        friend
        std::size_t
        write_some(
                net::basic_stream_socket<Protocol, Executor>& sock,
                serializer<isRequest,
                        basic_file_body<file_posix>, Fields>& sr,
                error_code& ec);

        value_type& body_;  // The body we are reading from
        std::uint64_t pos_; // The current position in the file
        char buf_[BOOST_BEAST_FILE_BODY_CHUNK_SIZE];    // Small buffer for reading

      public:
        using const_buffers_type =
        net::const_buffer;

        template<bool isRequest, class Fields>
        writer(header<isRequest, Fields>&, value_type& b)
                : body_(b)
                , pos_(body_.first_)
        {
            BOOST_ASSERT(body_.file_.is_open());
        }

        void
        init(error_code& ec)
        {
            BOOST_ASSERT(body_.file_.is_open());
            ec.clear();
        }

        boost::optional<std::pair<const_buffers_type, bool>>
        get(error_code& ec)
        {
            std::size_t const n = (std::min)(sizeof(buf_),
                                             beast::detail::clamp(body_.last_ - pos_));
            if(n == 0)
            {
                ec = {};
                return boost::none;
            }
            auto const nread = body_.file_.read(buf_, n, ec);
            if(ec)
                return boost::none;
            if (nread == 0)
            {
                ec = error::short_read;
                return boost::none;
            }
            BOOST_ASSERT(nread != 0);
            pos_ += nread;
            ec = {};
            return {{{buf_, nread},          // buffer to return.
                     pos_ < body_.last_}};   // `true` if there are more buffers.
        }
    };

    //--------------------------------------------------------------------------

    class reader
    {
        value_type& body_;

      public:
        template<bool isRequest, class Fields>
        explicit
        reader(header<isRequest, Fields>&, value_type& b)
                : body_(b)
        {
        }

        void
        init(boost::optional<
                std::uint64_t> const& content_length,
             error_code& ec)
        {
            // VFALCO We could reserve space in the file
            boost::ignore_unused(content_length);
            BOOST_ASSERT(body_.file_.is_open());
            ec = {};
        }

        template<class ConstBufferSequence>
        std::size_t
        put(ConstBufferSequence const& buffers,
            error_code& ec)
        {
            std::size_t nwritten = 0;
            for(auto buffer : beast::buffers_range_ref(buffers))
            {
                nwritten += body_.file_.write(
                        buffer.data(), buffer.size(), ec);
                if(ec)
                    return nwritten;
            }
            ec = {};
            return nwritten;
        }

        void
        finish(error_code& ec)
        {
            ec = {};
        }
    };

    //--------------------------------------------------------------------------

    static
    std::uint64_t
    size(value_type const& body)
    {
        return body.size();
    }
};

//------------------------------------------------------------------------------

inline
void
basic_file_body<file_posix>::
value_type::
close()
{
    error_code ignored;
    file_.close(ignored);
}

inline
void
basic_file_body<file_posix>::
value_type::
open(char const* path, file_mode mode, error_code& ec)
{
    file_.open(path, mode, ec);
    if(ec)
        return;
    size_ = file_.size(ec);
    if(ec)
    {
        close();
        return;
    }
    first_ = 0;
    last_ = size_;
}

inline
void
basic_file_body<file_posix>::
value_type::
reset(file_posix && file, error_code& ec)
{
    if(file_.is_open())
    {
        error_code ignored;
        file_.close(ignored);
    }
    file_ = std::move(file);
    if(file_.is_open())
    {
        size_ = file_.size(ec);
        if(ec)
        {
            close();
            return;
        }
        first_ = 0;
        last_ = size_;
    }
}

//------------------------------------------------------------------------------

template<
        class Protocol, class Executor,
        bool isRequest, class Fields>
std::size_t
write_some(
        net::basic_stream_socket<
                Protocol, Executor>& sock,
        serializer<isRequest,
        basic_file_body<file_posix>, Fields>& sr,
        error_code& ec)
{
    if(! sr.is_header_done())
    {
        sr.split(true);
        auto const bytes_transferred =
                detail::write_some_impl(sock, sr, ec);
        if(ec)
            return bytes_transferred;
        return bytes_transferred;
    }
    if(sr.get().chunked())
    {
        auto const bytes_transferred =
                detail::write_some_impl(sock, sr, ec);
        if(ec)
            return bytes_transferred;
        return bytes_transferred;
    }
    auto& w = sr.writer_impl();
    if(ec)
        return 0;

    off_t off = w.pos_;

    auto res = detail::call_sendfile(sock.native_handle(),
                                     w.body_.file_.native_handle(),
                                     off, (std::min)(w.body_.size_ - off, sr.limit()));
    if(res == static_cast<ssize_t>(-1))
    {
        // the file can't do nonblock anyhow, so only the socket can block
        if (errno == EAGAIN)
        {
            sock.wait(net::socket_base::wait_write, ec);
            if (ec)
                return 0;
        }
        ec = detail::make_sendfile_error(errno);
        return 0;
    }
    else if (res == 0) // we're done
    {
        sr.next(ec, detail::null_lambda{});
        BOOST_ASSERT(! ec);
        BOOST_ASSERT(sr.is_done());
        return 0u;
    }
    else
    {
        w.pos_ = off;
        return res;
    }
}

namespace detail {


template<
        class Protocol, class Executor,
        bool isRequest, class Fields,
        class Handler>
class write_some_posix_op
        : public beast::async_base<Handler, Executor>
{
    net::basic_stream_socket<
            Protocol, Executor>& sock_;
    serializer<isRequest,
            basic_file_body<file_posix>, Fields>& sr_;
    bool header_ = false;

  public:
    template<class Handler_>
    write_some_posix_op(
            Handler_&& h,
            net::basic_stream_socket<
                    Protocol, Executor>& s,
            serializer<isRequest,
                    basic_file_body<file_posix>,Fields>& sr)
            : async_base<
            Handler, Executor>(
            std::forward<Handler_>(h),
            s.get_executor())
            , sock_(s)
            , sr_(sr)
    {
        (*this)();
    }

    void
    operator()()
    {
        if(! sr_.is_header_done())
        {
            header_ = true;
            sr_.split(true);
            return detail::async_write_some_impl(
                    sock_, sr_, std::move(*this));
        }
        if(sr_.get().chunked())
        {
            return detail::async_write_some_impl(
                    sock_, sr_, std::move(*this));
        }
        // we always do a wait, as to complete as by post
        sock_.async_wait(net::socket_base::wait_write, std::move(*this));
    }

    void
    operator()(
            error_code ec,
            std::size_t sz)
    {
        return this->complete_now(ec, sz);
    }

    void
    operator()(
            error_code ec)
    {
        if(ec)
            return this->complete_now(ec, 0u);

        auto& w = sr_.writer_impl();
        off_t off = w.pos_;

        const auto res = detail::call_sendfile(sock_.native_handle(),
                                         sr_.get().body().file_.native_handle(),
                                         off, (std::min)(sr_.get().body().size() - off, sr_.limit()));

        if(res == static_cast<ssize_t>(-1))
        {
            // the file can't do nonblock anyhow, so only the socket can block
            if (errno == EAGAIN)
                return sock_.async_wait(net::socket_base::wait_write, std::move(*this));
            this->complete_now(detail::make_sendfile_error(errno), 0u);
        }
        else if (res == 0) // we're done
        {
            sr_.next(ec, detail::null_lambda{});
            BOOST_ASSERT(! ec);
            BOOST_ASSERT(sr_.is_done());
        }
        else
            w.pos_ = off;

        this->complete_now(error_code{}, res);
    }
};

struct run_write_some_posix_op
{
    template<
            class Protocol, class Executor,
            bool isRequest, class Fields,
            class WriteHandler>
    void
    operator()(
            WriteHandler&& h,
            net::basic_stream_socket<
                    Protocol, Executor>* s,
            serializer<isRequest,
                    basic_file_body<file_posix>, Fields>* sr)
    {
        // If you get an error on the following line it means
        // that your handler does not meet the documented type
        // requirements for the handler.
    
        static_assert(
                beast::detail::is_invocable<WriteHandler,
                        void(error_code, std::size_t)>::value,
                "WriteHandler type requirements not met");
    
        write_some_posix_op<
                Protocol, Executor,
                isRequest, Fields,
                typename std::decay<WriteHandler>::type>(
                std::forward<WriteHandler>(h), *s, *sr);
    }
};



}

template<
        class Protocol, class Executor,
        bool isRequest, class Fields,
        BOOST_BEAST_ASYNC_TPARAM2 WriteHandler>
BOOST_BEAST_ASYNC_RESULT2(WriteHandler)
async_write_some(
        net::basic_stream_socket<
                Protocol, Executor>& sock,
        serializer<isRequest,
                basic_file_body<file_posix>, Fields>& sr,
        WriteHandler&& handler)
{
    return net::async_initiate<
            WriteHandler,
            void(error_code, std::size_t)>(
            detail::run_write_some_posix_op{},
            handler,
            &sock,
            &sr);
}


}
}
}


#endif
#endif

#endif //BOOST_BEAST_HTTP_IMPL_FILE_BODY_POSIX_HPP
