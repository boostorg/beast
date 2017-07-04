//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_FILE_BODY_LINUX_HPP
#define BEAST_HTTP_FILE_BODY_LINUX_HPP

#ifndef BEAST_NO_LINUX_FILE
# ifdef __linux__
#  define BEAST_NO_LINUX_FILE 0
# else
#  define BEAST_NO_LINUX_FILE 1
# endif
#endif

#if ! BEAST_NO_LINUX_FILE

#include <beast/core/async_result.hpp>
#include <beast/core/error.hpp>
#include <beast/core/file_base.hpp>
#include <beast/http/message.hpp>
#include <beast/http/serializer.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/filesystem.hpp>
#include <csignal>
#include <sys/sendfile.h>
#include <unistd.h>

namespace beast {
namespace http {

struct file_body_linux
{
    class reader;
    class writer;

    class value_type
    {
        int file_{-1};
        std::uint64_t size_{0};

        friend class reader;
        friend class writer;

        template<int Signal>
        static void signal_init()
        {
            static auto x{std::signal(Signal, SIG_IGN)};
            (void)x;
        }

    public:
        value_type(const value_type&) = delete;
        value_type& operator=(const value_type&) = delete;

        /** Destructor.

            If the file handle is open, it is closed first.
        */
        ~value_type()
        {
            if(file_ != -1)
                ::close(file_);
        }

        /// Default constructor
        value_type() = default;

        /// Move constructor
        value_type(value_type&& other) noexcept
            : file_(other.file_)
            , size_(other.size_)
        {
            other.file_ = -1;
            other.size_ = 0;
        }

        /// Move assignment
        value_type& operator=(value_type&& other) noexcept
        {
            value_type tmp(std::move(other));
            std::swap(file_, tmp.file_);
            std::swap(size_, tmp.size_);
            return *this;
        }

        /// Returns the native file handle
        int
        native_handle() const
        {
            return file_;
        }

        /// Returns `true` if the file is open
        bool
        is_open() const
        {
            return file_ != -1;
        }

        /** Open a file for reading or writing.

            @param path The path to the file

            @param mode The open mode used with fopen()

            @param ec Set to the error, if any occurred
        */
        void
        open(boost::filesystem::path const& path,
            file_mode mode, error_code& ec)
        {
            int f;
            switch(mode)
            {
            default:
            case file_mode::read:   f = O_RDONLY; break;
            case file_mode::scan:   f = O_RDONLY; break;
            case file_mode::write:  f = O_WRONLY | O_CREAT; break;
            case file_mode::append: f = O_WRONLY | O_APPEND; break;
            }
            file_ = ::open(path.string().c_str(), f, 0777);
            if(file_ == -1)
            {
                ec.assign(errno, beast::system_category());
                return;
            }
            struct stat s;
            if(::fstat(file_, &s) == -1)
            {
                ec.assign(errno, beast::system_category());
                ::close(file_);
                file_ = -1;
                return;
            }
            size_ = s.st_size;
            ec.assign(0, ec.category());
            signal_init<SIGPIPE>();
        }

        /** Returns the size of the file.

            The file must already be open.

            @return The size of the file if a file is open, else undefined.
        */
        std::uint64_t
        size() const
        {
            return size_;
        }
    };

    // Returns the content length of the body in a message.
    static
    std::uint64_t
    size(value_type const& v)
    {
        return v.size();
    }

#if BEAST_DOXYGEN
    class reader;
#else
    class reader
    {
        int file_;
        std::uint64_t offset_ = 0;
        std::uint64_t size_ = 0;
        char buf_[4096];

    public:
        using const_buffers_type =
            boost::asio::const_buffers_1;

        template<bool isRequest, class Fields>
        reader(beast::http::message<isRequest,
            file_body_linux, Fields> const& m,
                beast::error_code& ec)
            : file_(m.body.file_)
        {
            BOOST_ASSERT(file_ != -1);
            size_ = m.body.size_;
            ec.assign(0, ec.category());
        }

        auto
        get(beast::error_code& ec) ->
            boost::optional<std::pair<const_buffers_type, bool>>
        {
            auto const remain =
                static_cast<std::uint64_t>(size_ - offset_);
            auto const amount = remain > sizeof(buf_) ?
                sizeof(buf_) : static_cast<std::size_t>(remain);
            if(amount == 0)
            {
                ec.assign(0, ec.category());
                return boost::none;
            }
            ssize_t n = ::pread(file_, buf_, amount, offset_);
            if(n < 0)
            {
                ec.assign(errno, beast::system_category());
                return boost::none;
            }
            else if(n == 0)
            {
                ec.assign(beast::errc::invalid_seek,
                    beast::system_category());
                return boost::none;
            }
            offset_ += amount;
            ec.assign(0, ec.category());
            return {{
                const_buffers_type{buf_, amount},
                remain > amount
                }};
        }
    };
#endif
};

#if ! BEAST_DOXYGEN

template<class Protocol, bool isRequest, class Fields>
void
write(boost::asio::basic_stream_socket<Protocol>& socket,
    serializer<isRequest, file_body_linux, Fields>& sr);

template<class Protocol, bool isRequest, class Fields>
void
write(boost::asio::basic_stream_socket<Protocol>& socket,
    serializer<isRequest, file_body_linux, Fields>& sr,
        error_code& ec);

template<class Protocol,
    bool isRequest, class Fields, class Decorator,
        class WriteHandler>
async_return_type<WriteHandler, void(error_code)>
async_write(boost::asio::basic_stream_socket<Protocol>& socket,
    serializer<isRequest, file_body_linux,
        Fields, Decorator>& sr,
            WriteHandler&& handler);

#endif // BEAST_DOXYGEN

} // http
} // beast

#include <beast/http/impl/file_body_linux.ipp>

#endif

#endif
