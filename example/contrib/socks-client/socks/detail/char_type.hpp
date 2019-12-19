//
// Copyright (c) 2018 jackarain (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef SOCKS_DETAIL_CHAR_TYPE_HPP
#define SOCKS_DETAIL_CHAR_TYPE_HPP

#include <socks/config.hpp>
#include <string>
#include <stdexcept>
#include <iterator>

namespace socks {
namespace detail {

inline
bool
isdigit(const char c) noexcept
{
    return c >= '0' && c <= '9';
}

inline
bool
isalpha(const char c) noexcept
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

inline
bool
ishexdigit(const char c) noexcept
{
    return isdigit(c) || ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
}

inline
bool
isunreserved(const char c) noexcept
{
    if (isalpha(c) || isdigit(c) ||
        c == '-' || c == '.' ||
        c == '_' || c == '~')
        return true;
    return false;
}

inline
bool
isuchar(const char c) noexcept
{
    if (isunreserved(c) ||
        c == ';' || c == '?' || c == '&' || c == '=')
        return true;
    return false;
}

inline
bool
ishsegment(const char c) noexcept
{
    if (isuchar(c) ||
        c == ';' || c == ':' ||
        c == '@' || c == '&' || c == '=')
        return true;
    return false;
}

inline
bool
issubdelims(const char c) noexcept
{
    if (c == '!' || c == '$' || c == '\'' || c == '(' ||
        c == ')' || c == '*' || c == '+'  || c == ',' ||
        c == '=')
        return true;
    return false;
}

inline
bool
uri_reserved(const char c) noexcept
{
    if (c == ';' || c == '/' || c == '?' || c == ':' ||
        c == '@' || c == '&' || c == '=' || c == '+' ||
        c == '$' || c == ',')
        return true;
    return false;
}

inline
bool
uri_mark(const char c) noexcept
{
    if (c == '-' || c == '_' || c == '.'  || c == '!' ||
        c == '~' || c == '*' || c == '\'' || c == '(' ||
        c == ')')
        return true;
    return false;
}

inline
string_view
to_hex(unsigned char c) noexcept
{
  static const char hexstring[] = {
    "00" "01" "02" "03" "04" "05" "06" "07" "08" "09" "0a" "0b" "0c" "0d" "0e" "0f"
    "10" "11" "12" "13" "14" "15" "16" "17" "18" "19" "1a" "1b" "1c" "1d" "1e" "1f"
    "20" "21" "22" "23" "24" "25" "26" "27" "28" "29" "2a" "2b" "2c" "2d" "2e" "2f"
    "30" "31" "32" "33" "34" "35" "36" "37" "38" "39" "3a" "3b" "3c" "3d" "3e" "3f"
    "40" "41" "42" "43" "44" "45" "46" "47" "48" "49" "4a" "4b" "4c" "4d" "4e" "4f"
    "50" "51" "52" "53" "54" "55" "56" "57" "58" "59" "5a" "5b" "5c" "5d" "5e" "5f"
    "60" "61" "62" "63" "64" "65" "66" "67" "68" "69" "6a" "6b" "6c" "6d" "6e" "6f"
    "70" "71" "72" "73" "74" "75" "76" "77" "78" "79" "7a" "7b" "7c" "7d" "7e" "7f"
    "80" "81" "82" "83" "84" "85" "86" "87" "88" "89" "8a" "8b" "8c" "8d" "8e" "8f"
    "90" "91" "92" "93" "94" "95" "96" "97" "98" "99" "9a" "9b" "9c" "9d" "9e" "9f"
    "a0" "a1" "a2" "a3" "a4" "a5" "a6" "a7" "a8" "a9" "aa" "ab" "ac" "ad" "ae" "af" 
    "b0" "b1" "b2" "b3" "b4" "b5" "b6" "b7" "b8" "b9" "ba" "bb" "bc" "bd" "be" "bf"
    "c0" "c1" "c2" "c3" "c4" "c5" "c6" "c7" "c8" "c9" "ca" "cb" "cc" "cd" "ce" "cf" 
    "d0" "d1" "d2" "d3" "d4" "d5" "d6" "d7" "d8" "d9" "da" "db" "dc" "dd" "de" "df" 
    "e0" "e1" "e2" "e3" "e4" "e5" "e6" "e7" "e8" "e9" "ea" "eb" "ec" "ed" "ee" "ef"
    "f0" "f1" "f2" "f3" "f4" "f5" "f6" "f7" "f8" "f9" "fa" "fb" "fc" "fd" "fe" "ff" 
  };

  auto offset = unsigned(c) * 2;

  return string_view(hexstring + offset, 2);
}

inline
int64_t
from_string(string_view str, int base = -1)
{
    const char* start = str.data();
    const char* end = str.data() + str.size();

    if (start >= end)
        return 0;

    bool has_prefix = false;

    if (*start == '0')
    {
        if (base == -1)
            base = 8;

        if (end - start >= 2 &&
            boost::beast::detail::ascii_tolower(*(start + 1)) == 'x')
        {
            if (base == -1)
                base = 16;
            has_prefix = true;
        }
    }

    if (base == -1)
        base = 10;

    if (base == 16 && has_prefix)
        start += 2;

    const char* p = start;
    while (p < end)
    {
        const char c = *p++;
        switch (base)
        {
        case 8:
            if (c < '0' || c > '7')
                return -1;
            continue;
        case 10:
            if (!isdigit(c))
                return -1;
            continue;
        case 16:
            if (!ishexdigit(c))
                return -1;
            continue;
        }
    }

    return strtoll(start, nullptr, base);
}


} // detail
} // socks

#endif
