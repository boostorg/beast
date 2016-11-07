//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_WRITE_HPP
#define BEAST_HTTP_WRITE_HPP

#include <beast/http/message.hpp>
#include <beast/core/error.hpp>
#include <beast/core/async_completion.hpp>
#include <ostream>
#include <type_traits>

namespace beast {
namespace http {

/** Write HTTP/1 message headers on a stream.

    This function is used to write message headers to a stream. The
    call will block until one of the following conditions is true:

    @li All the message headers are sent.

    @li An error occurs.

    This operation is implemented in terms of one or more calls
    to the stream's `write_some` function.

    Regardless of the semantic meaning of the headers (for example,
    specifying a zero-length message body and Connection: Close),
    this function will not return `boost::asio::error::eof`.

    @param stream The stream to which the data is to be written.
    The type must support the @b `SyncWriteStream` concept.

    @param msg The message headers to write.

    @throws system_error Thrown on failure.
*/
template<class SyncWriteStream,
    bool isRequest, class Headers>
void
write(SyncWriteStream& stream,
    message_headers<isRequest, Headers> const& msg);

/** Write HTTP/1 message headers on a stream.

    This function is used to write message headers to a stream. The
    call will block until one of the following conditions is true:

    @li All the message headers are sent.

    @li An error occurs.

    This operation is implemented in terms of one or more calls
    to the stream's `write_some` function.

    Regardless of the semantic meaning of the headers (for example,
    specifying a zero-length message body and Connection: Close),
    this function will not return `boost::asio::error::eof`.

    @param stream The stream to which the data is to be written.
    The type must support the @b `SyncWriteStream` concept.

    @param msg The message headers to write.

    @param ec Set to the error, if any occurred.
*/
template<class SyncWriteStream,
    bool isRequest, class Headers>
void
write(SyncWriteStream& stream,
    message_headers<isRequest, Headers> const& msg,
        error_code& ec);

/** Start an asynchronous operation to write HTTP/1 message headers to a stream.

    This function is used to asynchronously write message headers to a stream.
    The function call always returns immediately. The asynchronous
    operation will continue until one of the following conditions is true:

    @li The entire message headers are sent.

    @li An error occurs.

    This operation is implemented in terms of one or more calls to the
    stream's `async_write_some` functions, and is known as a <em>composed
    operation</em>. The program must ensure that the stream performs no
    other write operations (such as @ref async_write, the stream's
    `async_write_some` function, or any other composed operations that
    perform writes) until this operation completes.

    Regardless of the semantic meaning of the headers (for example,
    specifying a zero-length message body and Connection: Close),
    the handler will not be called with `boost::asio::error::eof`.

    @param stream The stream to which the data is to be written.
    The type must support the @b `AsyncWriteStream` concept.

    @param msg The message headers to send.

    @param handler The handler to be called when the request completes.
    Copies will be made of the handler as required. The equivalent
    function signature of the handler must be:
    @code void handler(
        error_code const& error // result of operation
    ); @endcode
    Regardless of whether the asynchronous operation completes
    immediately or not, the handler will not be invoked from within
    this function. Invocation of the handler will be performed in a
    manner equivalent to using `boost::asio::io_service::post`.

    @note The message object must remain valid at least until the
          completion handler is called, no copies are made.
*/
template<class AsyncWriteStream,
    bool isRequest, class Headers,
        class WriteHandler>
#if GENERATING_DOCS
void_or_deduced
#else
typename async_completion<
    WriteHandler, void(error_code)>::result_type
#endif
async_write(AsyncWriteStream& stream,
    message_headers<isRequest, Headers> const& msg,
        WriteHandler&& handler);

//------------------------------------------------------------------------------

/** Write a HTTP/1 message on a stream.

    This function is used to write a message to a stream. The call
    will block until one of the following conditions is true:

    @li The entire message is sent.

    @li An error occurs.

    This operation is implemented in terms of one or more calls
    to the stream's `write_some` function.

    The implementation will automatically perform chunk encoding if
    the contents of the message indicate that chunk encoding is required.
    If the semantics of the message indicate that the connection should
    be closed after the message is sent, the error thrown from this
    function will be `boost::asio::error::eof`.

    @param stream The stream to which the data is to be written.
    The type must support the @b `SyncWriteStream` concept.

    @param msg The message to write.

    @throws system_error Thrown on failure.
*/
template<class SyncWriteStream,
    bool isRequest, class Body, class Headers>
void
write(SyncWriteStream& stream,
    message<isRequest, Body, Headers> const& msg);

/** Write a HTTP/1 message on a stream.

    This function is used to write a message to a stream. The call
    will block until one of the following conditions is true:

    @li The entire message is sent.

    @li An error occurs.

    This operation is implemented in terms of one or more calls
    to the stream's `write_some` function.

    The implementation will automatically perform chunk encoding if
    the contents of the message indicate that chunk encoding is required.
    If the semantics of the message indicate that the connection should
    be closed after the message is sent, the error returned from this
    function will be `boost::asio::error::eof`.

    @param stream The stream to which the data is to be written.
    The type must support the @b `SyncWriteStream` concept.

    @param msg The message to write.

    @param ec Set to the error, if any occurred.
*/
template<class SyncWriteStream,
    bool isRequest, class Body, class Headers>
void
write(SyncWriteStream& stream,
    message<isRequest, Body, Headers> const& msg,
        error_code& ec);

/** Start an asynchronous operation to write a HTTP/1 message to a stream.

    This function is used to asynchronously write a message to a stream.
    The function call always returns immediately. The asynchronous
    operation will continue until one of the following conditions is true:

    @li The entire message is sent.

    @li An error occurs.

    This operation is implemented in terms of one or more calls to the
    stream's `async_write_some` functions, and is known as a <em>composed
    operation</em>. The program must ensure that the stream performs no
    other write operations (such as @ref async_write, the stream's
    `async_write_some` function, or any other composed operations that
    perform writes) until this operation completes.

    The implementation will automatically perform chunk encoding if
    the contents of the message indicate that chunk encoding is required.
    If the semantics of the message indicate that the connection should
    be closed after the message is sent, the operation will complete with
    the error set to `boost::asio::error::eof`.

    @param stream The stream to which the data is to be written.
    The type must support the @b `AsyncWriteStream` concept.

    @param msg The message to send.

    @param handler The handler to be called when the request completes.
    Copies will be made of the handler as required. The equivalent
    function signature of the handler must be:
    @code void handler(
        error_code const& error // result of operation
    ); @endcode
    Regardless of whether the asynchronous operation completes
    immediately or not, the handler will not be invoked from within
    this function. Invocation of the handler will be performed in a
    manner equivalent to using `boost::asio::io_service::post`.

    @note The message object must remain valid at least until the
          completion handler is called, no copies are made.
*/
template<class AsyncWriteStream,
    bool isRequest, class Body, class Headers,
        class WriteHandler>
#if GENERATING_DOCS
void_or_deduced
#else
typename async_completion<
    WriteHandler, void(error_code)>::result_type
#endif
async_write(AsyncWriteStream& stream,
    message<isRequest, Body, Headers> const& msg,
        WriteHandler&& handler);

//------------------------------------------------------------------------------

/** Serialize HTTP/1 message headers to a `std::ostream`.

    The function converts the message headers to its HTTP/1
    serialized representation and stores the result in the output
    stream.

    @param os The output stream to write to.

    @param msg The message headers to write.
*/
template<bool isRequest, class Headers>
std::ostream&
operator<<(std::ostream& os,
    message_headers<isRequest, Headers> const& msg);

/** Serialize a HTTP/1 message to a `std::ostream`.

    The function converts the message to its HTTP/1 serialized
    representation and stores the result in the output stream.

    The implementation will automatically perform chunk encoding if
    the contents of the message indicate that chunk encoding is required.

    @param os The output stream to write to.

    @param msg The message to write.
*/
template<bool isRequest, class Body, class Headers>
std::ostream&
operator<<(std::ostream& os,
    message<isRequest, Body, Headers> const& msg);

} // http
} // beast

#include <beast/http/impl/write.ipp>

#endif
