//
// Copyright (c) 2018 oneiric (oneiric at i2pmail dot org)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_URI_PARSER_HPP
#define BOOST_BEAST_CORE_URI_PARSER_HPP

#include <boost/beast/core/string.hpp>
#include <boost/beast/experimental/core/uri/error.hpp>
#include <boost/beast/experimental/core/uri/detail/parser.hpp>

namespace boost {
namespace beast {
namespace uri   {

/** Parse URL based on RFC3986

    Hierarchical URL

      [scheme:][//[user[:pass]@]host[:port]][/path][?query][#fragment]

    @param in URL to parse into component parts
    @param out Buffer to hold parsed URL parts
    @param ec Error code for signalling errors
*/
template <class Input = string_view>
inline void parse_absolute_form(Input in, buffer &out, error_code &ec)
{
  ec.assign(0, ec.category());
  detail::parser_impl p;
  p.parse_absolute_form(in, out, ec);
}

} // namespace uri
} // namespace beast
} // namespace boost

#endif  // BOOST_BEAST_CORE_URI_PARSER_HPP
