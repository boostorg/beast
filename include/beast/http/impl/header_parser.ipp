//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_HEADER_PARSER_IPP
#define BEAST_HTTP_IMPL_HEADER_PARSER_IPP

namespace beast {
namespace http {

template<bool isRequest, class Fields>
template<class Arg0, class... ArgN, class>
header_parser<isRequest, Fields>::
header_parser(Arg0&& arg0, ArgN&&... argn)
    : h_(std::forward<Arg0>(arg0),
         std::forward<ArgN>(argn)...)
{
}

} // http
} // beast

#endif
