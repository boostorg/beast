//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_WEBSOCKET_RFC6455_HPP
#define BEAST_WEBSOCKET_RFC6455_HPP

#include <beast/config.hpp>
#include <beast/core/static_string.hpp>
#include <beast/http/message.hpp>
#include <array>
#include <cstdint>

namespace beast {
namespace websocket {

/** Returns `true` if the specified HTTP request is a WebSocket Upgrade.

    This function returns `true` when the passed HTTP Request
    indicates a WebSocket Upgrade. It does not validate the
    contents of the fields: it just trivially accepts requests
    which could only possibly be a valid or invalid WebSocket
    Upgrade message.

    Callers who wish to manually read HTTP requests in their
    server implementation can use this function to determine if
    the request should be routed to an instance of
    @ref websocket::stream.

    @param req The HTTP Request object to check.

    @return `true` if the request is a WebSocket Upgrade.
*/
template<class Fields>
bool
is_upgrade(beast::http::header<true, Fields> const& req);

/** WebSocket frame header opcodes. */
enum class opcode : std::uint8_t
{
    cont    = 0,
    text    = 1,
    binary  = 2,
    rsv3    = 3,
    rsv4    = 4,
    rsv5    = 5,
    rsv6    = 6,
    rsv7    = 7,
    close   = 8,
    ping    = 9,
    pong    = 10,
    crsvb   = 11,
    crsvc   = 12,
    crsvd   = 13,
    crsve   = 14,
    crsvf   = 15
};

/** Close status codes.

    These codes accompany close frames.

    @see <a href="https://tools.ietf.org/html/rfc6455#section-7.4.1">RFC 6455 7.4.1 Defined Status Codes</a>

*/
enum close_code
{
    /// Normal closure; the connection successfully completed whatever purpose for which it was created.
    normal          = 1000,

    /// The endpoint is going away, either because of a server failure or because the browser is navigating away from the page that opened the connection.
    going_away      = 1001,

    /// The endpoint is terminating the connection due to a protocol error.
    protocol_error  = 1002,

    /// The connection is being terminated because the endpoint received data of a type it cannot accept (for example, a text-only endpoint received binary data).
    unknown_data    = 1003,

    /// The endpoint is terminating the connection because a message was received that contained inconsistent data (e.g., non-UTF-8 data within a text message).
    bad_payload     = 1007,

    /// The endpoint is terminating the connection because it received a message that violates its policy. This is a generic status code, used when codes 1003 and 1009 are not suitable.
    policy_error    = 1008,

    /// The endpoint is terminating the connection because a data frame was received that is too large.
    too_big         = 1009,

    /// The client is terminating the connection because it expected the server to negotiate one or more extension, but the server didn't.
    needs_extension = 1010,

    /// The server is terminating the connection because it encountered an unexpected condition that prevented it from fulfilling the request.
    internal_error  = 1011,

    /// The server is terminating the connection because it is restarting.
    service_restart = 1012,

    /// The server is terminating the connection due to a temporary condition, e.g. it is overloaded and is casting off some of its clients.
    try_again_later = 1013,

    //----
    //
    // The following are illegal on the wire
    //

    /** Used internally to mean "no error"

        This code is reserved and may not be sent.
    */
    none            = 0,

    /** Reserved for future use by the WebSocket standard.

        This code is reserved and may not be sent.
    */
    reserved1       = 1004,

    /** No status code was provided even though one was expected.

        This code is reserved and may not be sent.
    */
    no_status       = 1005,

    /** Connection was closed without receiving a close frame
        
        This code is reserved and may not be sent.
    */
    abnormal        = 1006,

    /** Reserved for future use by the WebSocket standard.
        
        This code is reserved and may not be sent.
    */
    reserved2       = 1014,

    /** Reserved for future use by the WebSocket standard.
       
        This code is reserved and may not be sent.
    */
    reserved3       = 1015

    //
    //----

    //last = 5000 // satisfy warnings
};

/// The type representing the reason string in a close frame.
using reason_string = static_string<123, char>;

/// The type representing the payload of ping and pong messages.
using ping_data = static_string<125, char>;

/** Description of the close reason.

    This object stores the close code (if any) and the optional
    utf-8 encoded implementation defined reason string.
*/
struct close_reason
{
    /// The close code.
    std::uint16_t code = close_code::none;

    /// The optional utf8-encoded reason string.
    reason_string reason;

    /** Default constructor.

        The code will be none. Default constructed objects
        will explicitly convert to bool as `false`.
    */
    close_reason() = default;

    /// Construct from a code.
    close_reason(std::uint16_t code_)
        : code(code_)
    {
    }

    /// Construct from a reason. code is close_code::normal.
    template<std::size_t N>
    close_reason(char const (&reason_)[N])
        : code(close_code::normal)
        , reason(reason_)
    {
    }

    /// Construct from a code and reason.
    template<std::size_t N>
    close_reason(close_code code_,
            char const (&reason_)[N])
        : code(code_)
        , reason(reason_)
    {
    }

    /// Returns `true` if a code was specified
    operator bool() const
    {
        return code != close_code::none;
    }
};

} // websocket
} // beast

#include <beast/websocket/impl/rfc6455.ipp>

#endif
