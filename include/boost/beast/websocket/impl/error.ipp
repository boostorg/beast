//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_IMPL_ERROR_IPP
#define BOOST_BEAST_WEBSOCKET_IMPL_ERROR_IPP

namespace boost {
namespace beast {
namespace websocket {
namespace detail {

template<class>
string_view
error_codes::
get_message(error ev) const
{
    switch(ev)
    {
    default:
    case error::failed: return "WebSocket connection failed due to a protocol violation";
    case error::closed: return "WebSocket connection closed normally";
    case error::handshake_failed: return "WebSocket upgrade handshake failed";
    case error::buffer_overflow: return "WebSocket dynamic buffer overflow";
    case error::partial_deflate_block: return "WebSocket partial deflate block";
    }
}

} // detail
} // websocket
} // beast
} // boost

#endif
