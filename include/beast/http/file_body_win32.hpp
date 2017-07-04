//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_FILE_BODY_WIN32_HPP
#define BEAST_HTTP_FILE_BODY_WIN32_HPP

#include <beast/core/detail/win32_file.hpp>

#if ! BEAST_NO_WIN32_FILE

#include <beast/core/async_result.hpp>
#include <beast/core/error.hpp>
#include <beast/http/message.hpp>
#include <beast/http/serializer.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/windows/overlapped_ptr.hpp> // for BOOST_ASIO_HAS_WINDOWS_OVERLAPPED_PTR
#include <boost/filesystem.hpp>

namespace beast {
namespace http {

struct file_body_win32
{
    class reader;
    class writer;

    class value_type
    {
        beast::detail::win32_file file_;
        std::uint64_t size_;

        friend class reader;
        friend class writer;

    public:
        /** Destructor.

            If the file handle is open, it is closed first.
        */
        ~value_type() = default;

        /// Default constructor
        value_type() = default;

        /// Move constructor
        value_type(value_type&& other) = default;

        /// Move assignment
        value_type& operator=(value_type&& other) = default;

        /// Returns the native file handle
        HANDLE
        native_handle() const
        {
            return file_.native_handle();
        }

        /// Returns `true` if the file is open
        bool
        is_open() const
        {
            return file_.is_open();
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
            file_.open(mode, path.string(), ec);
            if(ec)
                return;
            size_ = file_.size(ec);
            if(ec)
                return;
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

#if BEAST_DOXYGEN
    class reader;
#else
    class reader
    {
        beast::detail::win32_file const& file_;
        std::uint64_t offset_ = 0;
        std::uint64_t size_ = 0;
        char buf_[4096];

    public:
        using const_buffers_type =
            boost::asio::const_buffers_1;

        template<bool isRequest, class Fields>
        reader(beast::http::message<isRequest,
            file_body_win32, Fields> const& m,
                beast::error_code& ec)
            : file_(m.body.file_)
        {
            BOOST_ASSERT(file_.is_open());
            size_ = file_.size(ec);
            if(ec)
                return;
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
            file_.read(offset_, buf_, amount, ec);
            if(ec)
                return boost::none;
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
    serializer<isRequest, file_body_win32, Fields>& sr);

template<class Protocol, bool isRequest, class Fields>
void
write(boost::asio::basic_stream_socket<Protocol>& socket,
    serializer<isRequest, file_body_win32, Fields>& sr,
        error_code& ec);

#if BOOST_ASIO_HAS_WINDOWS_OVERLAPPED_PTR

template<class Protocol,
    bool isRequest, class Fields, class Decorator,
        class WriteHandler>
async_return_type<WriteHandler, void(error_code)>
async_write(boost::asio::basic_stream_socket<Protocol>& socket,
    serializer<isRequest, file_body_win32,
        Fields, Decorator>& sr,
            WriteHandler&& handler);

#endif

#endif // BEAST_DOXYGEN

} // http
} // beast

#include <beast/http/impl/file_body_win32.ipp>

#endif

#endif
