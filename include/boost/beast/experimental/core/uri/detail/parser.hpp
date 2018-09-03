//
// Copyright (c) 2018 oneiric (oneiric at i2pmail dot org)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_URI_DETAIL_PARSER_HPP
#define BOOST_BEAST_CORE_URI_DETAIL_PARSER_HPP

#include <boost/beast/core/string.hpp>
#include <boost/beast/experimental/core/uri/buffer.hpp>
#include <boost/beast/experimental/core/uri/rfc3986.hpp>

namespace boost {
namespace beast {
namespace uri {
namespace detail {

class parser_impl {
  template <typename It>
  It find_delimiter_or_mismatch(It first, It last,
                                std::function<bool(char)> delimiter_func,
                                std::function<bool(char)> match_func,
                                const bool ending_segment, error_code &ec) {
    // Search for segment delimiter
    auto delimiter = std::find_if(first, last, delimiter_func);
    if (delimiter == last && !ending_segment) {
      ec = error::syntax;
      return delimiter;
    }
    // Search for characters that do not belong in the segment
    auto mismatch = std::find_if_not(first, delimiter, match_func);
    if (mismatch != delimiter) {
      ec = error::syntax;
      return mismatch;
    }
    return delimiter;
  }

  template <typename It>
  It append_decoded_or_char(It first, It last, buffer& out, error_code& ec)
  {
    if (*first == '%')
      {
        auto ch = pct_decode(++first, last, ec);
        if (ec)
          return first;
        out.push_back(ch);
        first += 2;
      }
    else
      out.push_back(*(first++));

    return first;
  }

  template <typename It>
  It parse_scheme(It first, It last, buffer &out, error_code &ec) {
    using beast::detail::ascii_tolower;

    /*  scheme         ; = ALPHA / *(ALPHA / DIGIT / "-" / "." / "+") / ":"
     */
    if (!is_alpha(*first)) {
      ec = error::syntax;
      return first;
    }
    const auto scheme_delimiter = [](const char c) { return c == ':'; };
    const auto is_scheme_char = [](const char c) {
      return is_alpha(c) || is_digit(c) || c == '-' || c == '.' || c == '+' ||
             c == ':';
    };
    constexpr bool ending_segment = false;
    auto delimeter = find_delimiter_or_mismatch(
        first, last, scheme_delimiter, is_scheme_char, ending_segment, ec);

    if (ec)
      return delimeter;

    while (first != delimeter)
      out.push_back(ascii_tolower(*(first++)));

    out.scheme(out.part_from(out.begin(), out.end()));
    out.push_back(*(first++));
    return first;
  }

  template <typename It>
  It parse_username(It first, It last, buffer &out, error_code &ec) {
    /*  username       ; = *(unreserved / pct-encoded / sub_delims) / ":" / "@"
     */
    const auto size = out.size();
    const auto username_delimiter = [](const char c) {
      return c == ':' || c == '@';
    };
    constexpr bool ending_segment = false;
    const auto delimeter = find_delimiter_or_mismatch(
        first, last, username_delimiter, is_pchar, ending_segment, ec);

    if (ec)
      return delimeter;

    while (first != delimeter) {
      first = append_decoded_or_char(first, last, out, ec);
      if (ec)
        return first;
    }
    const auto start = out.begin() + size;
    out.username(out.part_from(start, out.end()));
    out.push_back(*(first++));
    return first;
  }

  template <typename It>
  It parse_password(It first, It last, buffer &out, error_code &ec) {
    /*  password       ; = ":" / *(unreserved / pct-encoded / sub_delims) / "@"
     */
    const auto size = out.size();
    const auto password_delimiter = [](const char c) { return c == '@'; };
    constexpr bool ending_segment = false;
    auto delimeter = find_delimiter_or_mismatch(first, last, password_delimiter,
                                                is_pchar, ending_segment, ec);

    if (ec)
      return delimeter;

    while (first != delimeter) {
      first = append_decoded_or_char(first, last, out, ec);
      if (ec)
        return first;
    }
    const auto start = out.begin() + size;
    out.password(out.part_from(start, out.end()));
    out.push_back(*(first++));
    return first;
  }

  template <typename It>
  It parse_host(It first, It last, buffer &out, error_code &ec) {
    if (*first == '[') {
      first = parse_ipv6(first, last, out, ec);
      return first;
    } else { // Parse IPv4 or registered name
      first = parse_ipv4_reg(first, last, out, ec);
      return first;
    }
  }

  template <typename It>
  It parse_ipv6(It first, It last, buffer &out, error_code &ec) {
    /*  IP-literal = "[" ( IPv6address / IPvFuture  ) "]"
     *
     *  IPv6address =                              6( h16 ":" ) ls32
     *                /                       "::" 5( h16 ":" ) ls32
     *                / [               h16 ] "::" 4( h16 ":" ) ls32
     *                / [ *1( h16 ":" ) h16 ] "::" 3( h16 ":" ) ls32
     *                / [ *2( h16 ":" ) h16 ] "::" 2( h16 ":" ) ls32
     *                / [ *3( h16 ":" ) h16 ] "::"    h16 ":"   ls32
     *                / [ *4( h16 ":" ) h16 ] "::"              ls32
     *                / [ *5( h16 ":" ) h16 ] "::"              h16
     *                / [ *6( h16 ":" ) h16 ] "::"
     *
     *  TODO: need better IPv6 parsing.
     *  Current rule is serviceable, but doesn't cover all cases.
     */
    const auto size = out.size();
    const auto ipv6_delimiter = [](const char c) { return c == ']'; };
    const auto is_ipv6_char = [](const char c) {
      return c == ':' || is_hex(c);
    };
    constexpr bool ending_segment = false;
    // Pre-increment `first` to skip leading bracket
    const auto delimeter = find_delimiter_or_mismatch(
        ++first, last, ipv6_delimiter, is_ipv6_char, ending_segment, ec);

    if (ec)
      return delimeter;

    while (first != delimeter)
      out.push_back(*(first++));

    // Pre-increment `first` to skip trailing bracket
    if (++first != last && *first != ':' && *first != '/' && *first != '?' &&
        *first != '#') {
      ec = error::syntax;
      return first;
    }
    const auto start = out.begin() + size;
    out.host(out.part_from(start, out.end()));
    if (first != last)
      out.push_back(*first);

    return first;
  }

  template <typename It>
  It parse_ipv4_reg(It first, It last, buffer &out, error_code &ec) {
    /*  IPv4address = dec-octet "." dec-octet "." dec-octet "." dec-octet
     *
     *  dec-octet   = DIGIT                 ; 0-9
     *              / %x31-39 DIGIT         ; 10-99
     *              / "1" 2DIGIT            ; 100-199
     *              / "2" %x30-34 DIGIT     ; 200-249
     *              / "25" %x30-35          ; 250-255
     *
     *  reg-name    = *( unreserved / pct-encoded / sub-delims )
     *
     *  TODO: need distinct IPv4 parsing.
     *  Current rule captures IPv4, but is too loose, allowing invalid IPv4.
     */
    const auto size = out.size();
    const auto ipv4_delimiter = [](const char c) {
      return c == ':' || c == '/' || c == '?' || c == '#';
    };
    const auto is_ipv4_char = [](const char c) {
      return is_unreserved(c) || is_sub_delims(c) || c == '%';
    };
    constexpr bool ending_segment = true;
    const auto delimiter = find_delimiter_or_mismatch(
        first, last, ipv4_delimiter, is_ipv4_char, ending_segment, ec);

    if (ec)
      return delimiter;

    while (first != delimiter) {
      first = append_decoded_or_char(first, last, out, ec);
      if (ec)
        return first;
    }

    const auto start = out.begin() + size;
    out.host(out.part_from(start, out.end()));
    return first;
  }

  template <typename It>
  It parse_port(It first, It last, buffer &out, error_code &ec) {
    /*  port          ; = ":" / *DIGIT
     */
    if (*first != ':') {
      ec = error::syntax;
      return first;
    }
    out.push_back(*(first++));
    const auto size = out.size();
    const auto port_delimiter = [](const char c) {
      return c == '/' || c == '?' || c == '#';
    };
    constexpr bool ending_segment = true;
    const auto delimiter = find_delimiter_or_mismatch(
        first, last, port_delimiter, is_digit, ending_segment, ec);

    if (ec)
      return delimiter;

    while (first != delimiter)
      out.push_back(*(first++));

    const auto start = out.begin() + size;
    out.port(out.part_from(start, out.end()));
    if (first != last)
      out.push_back(*first);

    return first;
  }

  template <typename It>
  It parse_authority(It first, It last, buffer &out, error_code &ec) {
    if (first + 2 >= last || first[0] != '/' || first[1] != '/') {
      ec = error::syntax;
      return first;
    }
    out.push_back(*first++);
    out.push_back(*first++);
    if (out.scheme() == "file") {
      if (first != last && *first == '/')
        out.push_back(*first++);
      else {
        ec = error::syntax;
        return first;
      }
    }
    // Check for a username[:password] section
    const auto search = search_user_info(first, last, ec);
    if (!ec && *search == '@') {
      first = parse_username(first, last, out, ec);
      if (!ec && first != last && *(first - 1) == ':')
        first = parse_password(first, last, out, ec);
    }
    // Valid authority needs a host
    if ((ec && ec != error::mismatch) || first == last) {
      ec = error::syntax;
      return first;
    }
    // Clear a potential mismatch error
    ec.assign(0, ec.category());
    first = parse_host(first, last, out, ec);
    if (ec)
      return first;

    if (*first == ':')
      first = parse_port(first, last, out, ec);

    return first;
  }

  template <typename It>
  It search_user_info(It first, It last, error_code &ec) {
    const auto user_info_delimiter = [](const char c) { return c == '@'; };
    const auto is_user_info_char = [](const char c) {
      return c != '/' && c != '?' && c != '#' &&
             (is_uchar(c) || is_sub_delims(c) || c == '%' || c == ':');
    };
    constexpr bool ending_segment = false;
    first = find_delimiter_or_mismatch(first, last, user_info_delimiter,
                                       is_user_info_char, ending_segment, ec);

    if (ec)
      ec = error::mismatch;

    return first;
  }

  template <typename It>
  It parse_path(It first, It last, buffer &out, error_code &ec) {
    /*  path          ; = path-absolute = "/" [ segment-nz *( "/" segment ) ]
     *  segment       ; = *pchar
     *  segment-nz    ; = 1*pchar
     */
    const auto size = out.size();
    if (*first != '/') {
      ec = error::syntax;
      return first;
    }
    out.push_back(*(first++));
    // Path cannot start with "//", see spec
    if (*first == '/') {
      ec = error::syntax;
      return first;
    }
    const auto path_delimiter = [](const char c) {
      return c == '?' || c == '#';
    };
    const auto is_path_char = [](const char c) {
      return c == '/' || is_pchar(c);
    };
    constexpr bool ending_segment = true;
    const auto delimiter = find_delimiter_or_mismatch(
        first, last, path_delimiter, is_path_char, ending_segment, ec);

    if (ec)
      return delimiter;

    while (first != delimiter) {
      first = append_decoded_or_char(first, last, out, ec);
      if (ec)
        return first;
    }
    const auto start = out.begin() + size;
    out.path(out.part_from(start, out.end()));
    if (first != last)
      out.push_back(*first);

    return first;
  }

  template <typename It>
  It parse_query(It first, It last, buffer &out, error_code &ec) {
    /*  query         ; = "?" / *( pchar / "/" / "?" ) / "#"
     */
    if (*first != '?') {
      ec = error::syntax;
      return first;
    }
    out.push_back(*(first++));
    const auto size = out.size();
    const auto query_delimiter = [](const char c) { return c == '#'; };
    const auto is_query_char = [](const char c) {
      return is_pchar(c) || c == '/' || c == '?';
    };
    constexpr bool ending_segment = true;
    const auto delimiter = find_delimiter_or_mismatch(
        first, last, query_delimiter, is_query_char, ending_segment, ec);

    if (ec)
      return delimiter;

    while (first != delimiter) {
      first = append_decoded_or_char(first, last, out, ec);
      if (ec)
        return first;
    }
    if (out.size() == size)
      return first;

    const auto start = out.begin() + size;
    out.query(out.part_from(start, out.end()));
    return first;
  }

  template <typename It>
  void parse_fragment(It first, It last, buffer &out, error_code &ec) {
    /*  fragment      ; = = "#" / *( pchar / "/" / "?" )
     */
    if (*first != '#') {
      ec = error::syntax;
      return;
    }
    out.push_back(*(first++));
    const auto size = out.size();
    const auto is_fragment_char = [](const char c) {
      return is_pchar(c) || c == '/' || c == '?';
    };
    const auto mismatch = std::find_if_not(first, last, is_fragment_char);
    if (mismatch != last) {
      ec = error::syntax;
      return;
    }
    while (first != last) {
      first = append_decoded_or_char(first, last, out, ec);
      if (ec)
        return;
    }
    if (out.size() == size)
      return;

    const auto start = out.begin() + size;
    out.fragment(out.part_from(start, out.end()));
  }

public:
  template <typename Input = string_view>
  inline void parse_absolute_form(Input in, buffer &out, error_code &ec) {
    if (in.empty()) {
      ec = error::syntax;
      return;
    }
		auto first = in.begin();
		const auto last = in.end();
    first = parse_scheme(first, last, out, ec);
    if (ec || first == last) {
      ec = error::syntax;
      return;
    }
    first = parse_authority(first, last, out, ec);
    if (ec)
      return;
    if (*first == '/')
      first = parse_path(first, last, out, ec);
    if (ec)
      return;
    if (*first == '?')
      first = parse_query(first, last, out, ec);
    if (ec)
      return;
    if (*first == '#')
      parse_fragment(first, last, out, ec);
  }
};

} // detail

} // uri
} // beast
} // boost

#endif  // BOOST_BEAST_CORE_URI_DETAIL_PARSER_HPP
