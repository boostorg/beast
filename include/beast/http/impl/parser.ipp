//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_PARSER_IPP
#define BEAST_HTTP_IMPL_PARSER_IPP

#include <boost/throw_exception.hpp>
#include <stdexcept>

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

template<bool isRequest, class Body, class Fields>
template<class Arg1, class... ArgN, class>
parser<isRequest, Body, Fields>::
parser(Arg1&& arg1, ArgN&&... argn)
    : m_(std::forward<Arg1>(arg1),
        std::forward<ArgN>(argn)...)
{
}

template<bool isRequest, class Body, class Fields>
template<class OtherBody, class... Args>
parser<isRequest, Body, Fields>::
parser(parser<isRequest, OtherBody, Fields>&& parser,
        Args&&... args)
    : base_type(std::move(parser))
    , m_(parser.release().base(),
        std::forward<Args>(args)...)
{
    if(parser.wr_)
        BOOST_THROW_EXCEPTION(std::invalid_argument{
            "moved-from parser has a body"});
}

template<bool isRequest, class Body, class Fields>
template<class... Args>
parser<isRequest, Body, Fields>::
parser(header_parser<
    isRequest, Fields>&& parser, Args&&... args)
    : base_type(std::move(static_cast<basic_parser<
        isRequest, header_parser<isRequest, Fields>>&>(parser)))
    , m_(parser.release(), std::forward<Args>(args)...)
{
}

} // http
} // beast

#endif
