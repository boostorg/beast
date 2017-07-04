//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_FILE_BODY_LINUX_IPP
#define BEAST_HTTP_IMPL_FILE_BODY_LINUX_IPP

#include <beast/http/error.hpp>
#include <beast/http/write.hpp>
#include <memory>

namespace beast {
namespace http {

template<class Protocol, bool isRequest, class Fields>
void
write(boost::asio::basic_stream_socket<Protocol>& socket,
    serializer<isRequest, file_body_linux, Fields>& sr)
{
    error_code ec;
    write(socket, sr, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class Protocol, bool isRequest, class Fields>
void
write(boost::asio::basic_stream_socket<Protocol>& socket,
    serializer<isRequest, file_body_linux, Fields>& sr,
        error_code& ec)
{
    sr.split(true);
    write_header(socket, sr, ec);
    if(ec)
        return;
    for(off_t offset = 0;;)
    {
        // Try the system call.
        ssize_t n = ::sendfile(
            socket.native_handle(),
            sr.get().body.native_handle(),
            &offset,
            65536);
        ec.assign(
            n < 0 ? errno : 0,
            beast::system_category());

        // Retry immediately if interrupted by signal.
        if(ec == beast::errc::interrupted)
            continue;

        // Check for success.
        if(n == 0)
        {
            if(sr.need_close())
                ec = error::end_of_stream;
            return;
        }

        // Check for error.
        if(ec)
            return;

        // Loop around to try calling sendfile again.
    }
}

namespace detail {

template<
    class Protocol,
    bool isRequest, class Fields, class Decorator,
    class Handler>
class linux_write_op
{
    boost::asio::basic_stream_socket<Protocol>& s_;
    serializer<isRequest,
        file_body_linux, Fields, Decorator>& sr_;
    int file_{sr_.get().body.native_handle()};
    off_t offset_{0};
    Handler h_;

public:
    linux_write_op(linux_write_op&&) = default;
    linux_write_op(linux_write_op const&) = default;

    template<class DeducedHandler>
    linux_write_op(
        DeducedHandler&& h,
        boost::asio::basic_stream_socket<Protocol>& s,
        serializer<isRequest,
            file_body_linux, Fields, Decorator>& sr)
        : s_(s)
        , sr_(sr)
        , h_(std::forward<DeducedHandler>(h))
    {
    }

    void
    operator()();

    void
    operator()(error_code ec,
        std::size_t bytes_transferred = 0);

    friend
    void* asio_handler_allocate(
        std::size_t size, linux_write_op* op)
    {
        using boost::asio::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(op->h_));
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, linux_write_op* op)
    {
        using boost::asio::asio_handler_deallocate;
        asio_handler_deallocate(
            p, size, std::addressof(op->h_));
    }

    friend
    bool asio_handler_is_continuation(linux_write_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, linux_write_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(
            f, std::addressof(op->h_));
    }
};

template<class Protocol, bool isRequest,
    class Fields, class Decorator, class Handler>
void
linux_write_op<Protocol, isRequest,
    Fields, Decorator, Handler>::
operator()()
{
    async_write_header(s_, sr_, std::move(*this));
}

template<class Protocol, bool isRequest,
    class Fields, class Decorator, class Handler>
void
linux_write_op<Protocol, isRequest,
    Fields, Decorator, Handler>::
operator()(error_code ec, std::size_t)
{
    if(!ec)
    {
        // Put the underlying socket into non-blocking mode.
        if(!s_.native_non_blocking())
            s_.native_non_blocking(true, ec);

        for(;;)
        {
            // Try the system call.
            ssize_t n = ::sendfile(
                s_.native_handle(),
                file_,
                &offset_,
                65536);
            ec.assign(
                n < 0 ? errno : 0,
                beast::system_category());

            // Retry immediately if interrupted by signal.
            if(ec == beast::errc::interrupted)
                continue;

            // Check if we need to run the operation again.
            if(ec == beast::errc::operation_would_block
                || ec == beast::errc::resource_unavailable_try_again)
            {
                // Wait for socket to become ready again.
                s_.async_write_some(
                    boost::asio::null_buffers(),
                    *this);
                return;
            }

            if(ec || n == 0)
            {
                // An error occurred, or we have reached the
                // end of the file. Either way we must exit
                // the loop so we can call the handler.
                break;
            }

            // "Loop" around to try calling sendfile again.
            // Post to give another operation a chance to run.
            s_.get_io_service().post(*this);
            return;
        }

        // Pass result back to user's handler.
        h_(ec);
    }
}

} // detail

template<
    class Protocol,
    bool isRequest, class Fields, class Decorator,
    class WriteHandler>
async_return_type<
    WriteHandler, void(error_code)>
async_write(boost::asio::basic_stream_socket<Protocol>& socket,
    serializer<isRequest, file_body_linux, Fields,
        Decorator>& sr, WriteHandler&& handler)
{
    async_completion<WriteHandler,
        void(error_code)> init{handler};
    detail::linux_write_op<Protocol, isRequest,
        Fields, Decorator, handler_type<
            WriteHandler, void(error_code)>>{
                init.completion_handler, socket, sr}();
    return init.result.get();
}

} // http
} // beast

#endif
