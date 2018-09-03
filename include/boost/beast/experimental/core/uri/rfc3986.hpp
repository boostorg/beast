//
// Copyright (c) 2016-2018 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2018 oneiric (oneiric at i2pmail dot org)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_URI_DETAIL_RFC3986_HPP
#define BOOST_BEAST_CORE_URI_DETAIL_RFC3986_HPP

namespace boost {
namespace beast {
namespace uri {

bool is_alpha(char c) {
  unsigned constexpr a = 'a';
  return ((static_cast<unsigned>(c) | 32) - a) < 26U;
}

bool is_digit(char c) {
  unsigned constexpr zero = '0';
  return (static_cast<unsigned>(c) - zero) < 10;
}

bool is_hex(char c) {
  return (c == 'a' || c == 'b' || c == 'c' || c == 'd' || c == 'e' ||
          c == 'f' || c == 'A' || c == 'B' || c == 'C' || c == 'D' ||
          c == 'E' || c == 'F' || is_digit(c));
}

template<typename It>
inline char pct_decode(It first, It last, error_code& ec) {
  // Check iterators are in valid range, and next two characters are hex
  if (first + 2 > last || !is_hex(first[0]) || !is_hex(first[1])) {
    ec = error::syntax;
    return *first;
  }
  std::string s{first, 2};
  return static_cast<char>(std::stoi(s, nullptr, 16));
}

/*  gen-delims      = ":" / "/" / "?" / "#" / "[" / "]" / "@"
 */
bool is_gen_delims(char c) {
  return c == ':' || c == '/' || c == '?' || c == '#' || c == '[' || c == ']' ||
         c == '@';
}

/*  sub-delims      = "!" / "$" / "&" / "'" / "(" / ")"
                          / "*" / "+" / "," / ";" / "="
*/
bool is_sub_delims(char c) {
  return c == '!' || c == '$' || c == '&' || c == '\'' || c == '(' ||
         c == ')' || c == '*' || c == '+' || c == ',' || c == '=';
}

/*  reserved        = gen-delims / sub-delims
 */
bool is_reserved(char c) { return is_gen_delims(c) || is_sub_delims(c); }

/*  unreserved      = ALPHA / DIGIT / "-" / "." / "_" / "~"
 */
bool is_unreserved(char c) {
  return is_alpha(c) || is_digit(c) || c == '-' || c == '.' || c == '_' ||
         c == '~';
}

/*  pchar           = unreserved / pct-encoded / sub-delims / ":" / "@"
 */
bool is_pchar(char c) {
  return is_unreserved(c) || is_sub_delims(c) || c == '%' || c == ':' ||
         c == '@';
}

/*  qchar           = pchar / "/" / "?"
 */
bool is_qchar(char c) { return is_pchar(c) || c == '/' || c == '?'; }

/*  uchar           = unreserved / ";" / "?" / "&" / "="
 */
bool is_uchar(char c) {
  return is_unreserved(c) || c == ';' || c == '?' || c == '&' || c == '=';
}

/*  hsegment        = uchar / ":" / "@"
 */
bool is_hsegment(char c) { return is_uchar(c) || c == ':' || c == '@'; }

} // namespace uri
} // namespace beast
} // namespace boost

#endif
