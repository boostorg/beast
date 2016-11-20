//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_MESSAGE_PARSER_IPP
#define BEAST_HTTP_IMPL_MESSAGE_PARSER_IPP

namespace beast {
namespace http {

template<bool isRequest, class Body, class Fields>
template<class Arg1, class... ArgN, class>
message_parser<isRequest, Body, Fields>::
message_parser(Arg1&& arg1, ArgN&&... argn)
    : m_(std::forward<Arg1>(arg1),
        std::forward<ArgN>(argn)...)
{
}

template<bool isRequest, class Body, class Fields>
template<class... Args>
message_parser<isRequest, Body, Fields>::
message_parser(header_parser<
    isRequest, Fields>&& parser, Args&&... args)
    : base_type(std::move(static_cast<basic_parser<isRequest, header_parser<isRequest, Fields>>&>(parser)))
    , m_(parser.release(), std::forward<Args>(args)...)
{
    this->split(false);
}

} // http
} // beast

#endif
