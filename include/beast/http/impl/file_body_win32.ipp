//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_FILE_BODY_WIN32_IPP
#define BEAST_HTTP_IMPL_FILE_BODY_WIN32_IPP

#include <beast/http/error.hpp>
#include <beast/http/write.hpp>
#include <memory>

namespace beast {
namespace http {

template<class Protocol, bool isRequest, class Fields>
void
write(boost::asio::basic_stream_socket<Protocol>& socket,
    serializer<isRequest, file_body_win32, Fields>& sr)
{
    error_code ec;
    write(stream, sr, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class Protocol, bool isRequest, class Fields>
void
write(boost::asio::basic_stream_socket<Protocol>& socket,
    serializer<isRequest, file_body_win32, Fields>& sr,
        error_code& ec)
{
    sr.split(true);
    write_header(socket, sr, ec);
    if(ec)
        return;
    BOOL const bSuccess = ::TransmitFile(
        socket.native_handle(),
        sr.get().body.native_handle(),
        0,
        0,
        nullptr,
        nullptr,
        0);
    if(! bSuccess)
    {
        DWORD const dwError = ::GetLastError();
        ec = {static_cast<int>(dwError),
            system_category()};
        return;
    }
    if(sr.need_close())
        ec = error::end_of_stream;
}

#if BOOST_ASIO_HAS_WINDOWS_OVERLAPPED_PTR

namespace detail {

template<
    class Protocol,
    bool isRequest, class Fields, class Decorator,
    class Handler>
class win32_write_op
{
    boost::asio::basic_stream_socket<Protocol>& s_;
    serializer<isRequest,
        file_body_win32, Fields, Decorator>& sr_;
    Handler h_;

public:
    win32_write_op(win32_write_op&&) = default;
    win32_write_op(win32_write_op const&) = default;

    template<class DeducedHandler>
    win32_write_op(
        DeducedHandler&& h,
        boost::asio::basic_stream_socket<Protocol>& s,
        serializer<isRequest,
            file_body_win32, Fields, Decorator>& sr)
        : s_(s)
        , sr_(sr)
        , h_(std::forward<DeducedHandler>(h))
    {
    }

    void
    operator()();

    void
    operator()(error_code ec);

    void
    operator()(error_code ec,
        std::size_t bytes_transferred);

    friend
    void* asio_handler_allocate(
        std::size_t size, win32_write_op* op)
    {
        using boost::asio::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(op->h_));
    }

    friend
    void asio_handler_deallocate(
        void* p, std::size_t size, win32_write_op* op)
    {
        using boost::asio::asio_handler_deallocate;
        asio_handler_deallocate(
            p, size, std::addressof(op->h_));
    }

    friend
    bool asio_handler_is_continuation(win32_write_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, win32_write_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(
            f, std::addressof(op->h_));
    }
};

template<class Protocol, bool isRequest,
    class Fields, class Decorator, class Handler>
void
win32_write_op<Protocol, isRequest,
    Fields, Decorator, Handler>::
operator()()
{
    async_write_header(s_, sr_, std::move(*this));
}

template<class Protocol, bool isRequest,
    class Fields, class Decorator, class Handler>
void
win32_write_op<Protocol, isRequest,
    Fields, Decorator, Handler>::
operator()(error_code ec)
{
    if(! ec)
    {
        boost::asio::windows::overlapped_ptr overlapped(
            s_.get_io_service(), *this);
        BOOL const bSuccess = ::TransmitFile(
            s_.native_handle(),
            sr_.get().body.native_handle(),
            0,
            0,
            overlapped.get(),
            nullptr,
            0);
        DWORD dwError = ::GetLastError();
        if(! bSuccess && dwError != ERROR_IO_PENDING)
        {
            // completed immediately
            ec.assign(dwError, system_category());
            overlapped.complete(ec, 0);
            return;
        }
        overlapped.release();
        return;
    }
    h_(ec);
}

template<class Protocol, bool isRequest,
    class Fields, class Decorator, class Handler>
void
win32_write_op<Protocol, isRequest,
    Fields, Decorator, Handler>::
operator()(error_code ec, std::size_t)
{
    h_(ec);
}

} // detail

template<
    class Protocol,
    bool isRequest, class Fields, class Decorator,
    class WriteHandler>
async_return_type<
    WriteHandler, void(error_code)>
async_write(boost::asio::basic_stream_socket<Protocol>& socket,
    serializer<isRequest, file_body_win32, Fields,
        Decorator>& sr, WriteHandler&& handler)
{
    async_completion<WriteHandler,
        void(error_code)> init{handler};
    detail::win32_write_op<Protocol, isRequest,
        Fields, Decorator, handler_type<
            WriteHandler, void(error_code)>>{
                init.completion_handler, socket, sr}();
    return init.result.get();
}

#endif

} // http
} // beast

#endif
