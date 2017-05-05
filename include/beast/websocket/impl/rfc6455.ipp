//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_WEBSOCKET_IMPL_RFC6455_IPP
#define BEAST_WEBSOCKET_IMPL_RFC6455_IPP

#include <beast/http/rfc7230.hpp>

namespace beast {
namespace websocket {

template<class Fields>
bool
is_upgrade(http::header<true, Fields> const& req)
{
    if(req.version < 11)
        return false;
    if(req.method != "GET")
        return false;
    if(! http::is_upgrade(req))
        return false;
    if(! http::token_list{req.fields["Upgrade"]}.exists("websocket"))
        return false;
    if(! req.fields.exists("Sec-WebSocket-Version"))
        return false;
    return true;
}

} // websocket
} // beast

#endif
