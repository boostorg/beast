//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_WEBSOCKET_OPTION_HPP
#define BEAST_WEBSOCKET_OPTION_HPP

#include <beast/config.hpp>
#include <beast/websocket/rfc6455.hpp>
#include <beast/core/detail/type_traits.hpp>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace beast {
namespace websocket {

/** Automatic fragmentation option.

    Determines if outgoing message payloads are broken up into
    multiple pieces.

    When the automatic fragmentation size is turned on, outgoing
    message payloads are broken up into multiple frames no larger
    than the write buffer size.

    The default setting is to fragment messages.

    @note Objects of this type are used with
          @ref beast::websocket::stream::set_option.

    @par Example
    Setting the automatic fragmentation option:
    @code
    ...
    websocket::stream<ip::tcp::socket> stream(ios);
    stream.set_option(auto_fragment{true});
    @endcode
*/
#if BEAST_DOXYGEN
using auto_fragment = implementation_defined;
#else
struct auto_fragment
{
    bool value;

    explicit
    auto_fragment(bool v)
        : value(v)
    {
    }
};
#endif

/** Message type option.

    This controls the opcode set for outgoing messages. Valid
    choices are opcode::binary or opcode::text. The setting is
    only applied at the start when a caller begins a new message.
    Changing the opcode after a message is started will only
    take effect after the current message being sent is complete.

    The default setting is opcode::text.

    @note Objects of this type are used with
          @ref beast::websocket::stream::set_option.

    @par Example
    Setting the message type to binary.
    @code
    ...
    websocket::stream<ip::tcp::socket> ws(ios);
    ws.set_option(message_type{opcode::binary});
    @endcode
*/
#if BEAST_DOXYGEN
using message_type = implementation_defined;
#else
struct message_type
{
    opcode value;

    explicit
    message_type(opcode op)
    {
        if(op != opcode::binary && op != opcode::text)
            throw beast::detail::make_exception<std::invalid_argument>(
                "bad opcode", __FILE__, __LINE__);
        value = op;
    }
};
#endif

namespace detail {

using ping_cb = std::function<void(bool, ping_data const&)>;

} // detail

/** permessage-deflate extension options.

    These settings control the permessage-deflate extension,
    which allows messages to be compressed.

    @note Objects of this type are used with
          @ref beast::websocket::stream::set_option.
*/
struct permessage_deflate
{
    /// `true` to offer the extension in the server role
    bool server_enable = false;

    /// `true` to offer the extension in the client role
    bool client_enable = false;

    /** Maximum server window bits to offer

        @note Due to a bug in ZLib, this value must be greater than 8.
    */
    int server_max_window_bits = 15;

    /** Maximum client window bits to offer

        @note Due to a bug in ZLib, this value must be greater than 8.
    */
    int client_max_window_bits = 15;

    /// `true` if server_no_context_takeover desired
    bool server_no_context_takeover = false;

    /// `true` if client_no_context_takeover desired
    bool client_no_context_takeover = false;

    /// Deflate compression level 0..9
    int compLevel = 8;

    /// Deflate memory level, 1..9
    int memLevel = 4;
};

/** Ping callback option.

    Sets the callback to be invoked whenever a ping or pong is
    received during a call to one of the following functions:

    @li @ref beast::websocket::stream::read
    @li @ref beast::websocket::stream::read_frame
    @li @ref beast::websocket::stream::async_read
    @li @ref beast::websocket::stream::async_read_frame

    Unlike completion handlers, the callback will be invoked
    for each received ping and pong during a call to any
    synchronous or asynchronous read function. The operation is
    passive, with no associated error code, and triggered by reads.

    The signature of the callback must be:
    @code
    void
    callback(
        bool is_pong,               // `true` if this is a pong
        ping_data const& payload    // Payload of the pong frame
    );
    @endcode

    The value of `is_pong` will be `true` if a pong control frame
    is received, and `false` if a ping control frame is received.

    If the read operation receiving a ping or pong frame is an
    asynchronous operation, the callback will be invoked using
    the same method as that used to invoke the final handler.

    @note Objects of this type are used with
          @ref beast::websocket::stream::set_option.
          To remove the ping callback, construct the option with
          no parameters: `set_option(ping_callback{})`
*/
#if BEAST_DOXYGEN
using ping_callback = implementation_defined;
#else
struct ping_callback
{
    detail::ping_cb value;

    ping_callback() = default;
    ping_callback(ping_callback&&) = default;
    ping_callback(ping_callback const&) = default;

    explicit
    ping_callback(detail::ping_cb f)
        : value(std::move(f))
    {
    }
};
#endif

/** Read buffer size option.

    Sets the size of the read buffer used by the implementation to
    receive frames. The read buffer is needed when permessage-deflate
    is used.

    Lowering the size of the buffer can decrease the memory requirements
    for each connection, while increasing the size of the buffer can reduce
    the number of calls made to the next layer to read data.

    The default setting is 4096. The minimum value is 8.

    @note Objects of this type are used with
          @ref beast::websocket::stream::set_option.

    @par Example
    Setting the read buffer size.
    @code
    ...
    websocket::stream<ip::tcp::socket> ws(ios);
    ws.set_option(read_buffer_size{16 * 1024});
    @endcode
*/
#if BEAST_DOXYGEN
using read_buffer_size = implementation_defined;
#else
struct read_buffer_size
{
    std::size_t value;

    explicit
    read_buffer_size(std::size_t n)
        : value(n)
    {
        if(n < 8)
            throw beast::detail::make_exception<std::invalid_argument>(
                "read buffer size is too small", __FILE__, __LINE__);
    }
};
#endif

/** Maximum incoming message size option.

    Sets the largest permissible incoming message size. Message
    frame fields indicating a size that would bring the total
    message size over this limit will cause a protocol failure.

    The default setting is 16 megabytes. A value of zero indicates
    a limit of the maximum value of a `std::uint64_t`.

    @note Objects of this type are used with
          @ref beast::websocket::stream::set_option.

    @par Example
    Setting the maximum read message size.
    @code
    ...
    websocket::stream<ip::tcp::socket> ws(ios);
    ws.set_option(read_message_max{65536});
    @endcode
*/
#if BEAST_DOXYGEN
using read_message_max = implementation_defined;
#else
struct read_message_max
{
    std::size_t value;

    explicit
    read_message_max(std::size_t n)
        : value(n)
    {
    }
};
#endif

/** Write buffer size option.

    Sets the size of the write buffer used by the implementation to
    send frames. The write buffer is needed when masking payload data
    in the client role, compressing frames, or auto-fragmenting message
    data.

    Lowering the size of the buffer can decrease the memory requirements
    for each connection, while increasing the size of the buffer can reduce
    the number of calls made to the next layer to write data.

    The default setting is 4096. The minimum value is 8.

    The write buffer size can only be changed when the stream is not
    open. Undefined behavior results if the option is modified after a
    successful WebSocket handshake.

    @note Objects of this type are used with
          @ref beast::websocket::stream::set_option.

    @par Example
    Setting the write buffer size.
    @code
    ...
    websocket::stream<ip::tcp::socket> ws(ios);
    ws.set_option(write_buffer_size{8192});
    @endcode
*/
#if BEAST_DOXYGEN
using write_buffer_size = implementation_defined;
#else
struct write_buffer_size
{
    std::size_t value;

    explicit
    write_buffer_size(std::size_t n)
        : value(n)
    {
        if(n < 8)
            throw beast::detail::make_exception<std::invalid_argument>(
                "write buffer size is too small", __FILE__, __LINE__);
    }
};
#endif

} // websocket
} // beast

#endif
