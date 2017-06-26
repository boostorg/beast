//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_DETAIL_BASIC_PARSER_HPP
#define BEAST_HTTP_DETAIL_BASIC_PARSER_HPP

#include <beast/core/string.hpp>
#include <beast/http/error.hpp>
#include <beast/http/detail/rfc7230.hpp>
#include <boost/version.hpp>
#include <algorithm>
#include <cstddef>
#include <utility>

/*
    Portions of this file based on code from picophttpparser,
    copyright notice below.
        https://github.com/h2o/picohttpparser
*/
/*
 * Copyright (c) 2009-2014 Kazuho Oku, Tokuhiro Matsuno, Daisuke Murase,
 *                         Shigeo Mitsunari
 *
 * The software is licensed under either the MIT License (below) or the Perl
 * license.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

namespace beast {
namespace http {
namespace detail {

#if __GNUC__ >= 3
# define BEAST_LIKELY(x) __builtin_expect(!!(x), 1)
# define BEAST_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define BEAST_LIKELY(x) (x)
#define BEAST_UNLIKELY(x) (x)
#endif

class basic_parser_base
{
protected:
    enum class state
    {
        nothing_yet = 0,
        header,
        body0,
        body,
        body_to_eof0,
        body_to_eof,
        chunk_header0,
        chunk_header,
        chunk_body,
        complete
    };

    static
    bool
    is_pathchar(char c)
    {
        // VFALCO This looks the same as the one below...

        // TEXT = <any OCTET except CTLs, and excluding LWS>
        static bool constexpr tab[256] = {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //   0
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //  16
            0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  32
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  48
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  64
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  80
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  96
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, // 112
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 128
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 144
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 160
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 176
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 192
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 208
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 224
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1  // 240
        };
        return tab[static_cast<unsigned char>(c)];
    }

    static
    bool
    is_value_char(char c)
    {
        // any OCTET except CTLs and LWS
        static bool constexpr tab[256] = {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //   0
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //  16
            0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  32
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  48
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  64
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  80
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  96
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, // 112
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 128
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 144
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 160
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 176
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 192
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 208
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 224
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1  // 240
        };
        return tab[static_cast<unsigned char>(c)];
    }

    static
    inline
    bool
    is_text(char c)
    {
        // VCHAR / SP / HT / obs-text
        static bool constexpr tab[256] = {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, //   0
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //  16
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  32
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  48
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  64
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  80
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  96
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, // 112
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 128
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 144
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 160
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 176
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 192
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 208
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 224
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1  // 240
        };
        return tab[static_cast<unsigned char>(c)];
    }

    static
    inline
    bool
    unhex(unsigned char& d, char c)
    {
        static signed char constexpr tab[256] = {
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, //   0
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, //  16
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, //  32
             0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1, //  48
            -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1, //  64
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, //  80
            -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1, //  96
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1  // 112

            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1  // 128
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1  // 144
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1  // 160
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1  // 176
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1  // 192
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1  // 208
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1  // 224
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1  // 240
        };
        d = static_cast<unsigned char>(
            tab[static_cast<unsigned char>(c)]);
        return d != static_cast<unsigned char>(-1);
    }

    static
    bool
    is_digit(char c)
    {
        return static_cast<unsigned char>(c-'0') < 10;
    }

    static
    bool
    is_print(char c)
    {
        return static_cast<unsigned char>(c-33) < 94;
    }

    static
    string_view
    make_string(char const* first, char const* last)
    {
        return {first, static_cast<
            std::size_t>(last - first)};
    }

    template<class = void>
    static
    bool
    strieq(string_view s1,
        string_view s2)
    {
        if(s1.size() != s2.size())
            return false;
        auto p1 = s1.data();
        auto p2 = s2.data();
        for(auto n = s1.size(); n--; ++p1, ++p2)
            if(*p1 != tolower(*p2))
                return false;
        return true;
    }

    template<std::size_t N>
    bool
    strieq(const char (&s1)[N],
        string_view s2)
    {
        return strieq({s1, N-1}, s2);
    }

    template<class Iter, class Unsigned>
    static
    bool
    parse_dec(Iter it, Iter last, Unsigned& v)
    {
        if(! is_digit(*it))
            return false;
        v = *it - '0';
        for(;;)
        {
            if(! is_digit(*++it))
                break;
            auto const d = *it - '0';
            if(v > ((std::numeric_limits<
                    Unsigned>::max)() - 10) / 10)
                return false;
            v = 10 * v + d;
        }
        return it == last;
    }

    template<class Iter, class Unsigned>
    bool
    parse_hex(Iter& it, Unsigned& v)
    {
        unsigned char d;
        if(! unhex(d, *it))
            return false;
        v = d;
        for(;;)
        {
            if(! unhex(d, *++it))
                break;
            auto const v0 = v;
            v = 16 * v + d;
            if(v < v0)
                return false;
        }
        return true;
    }

    static
    bool
    parse_crlf(char const*& it)
    {
        if( it[0] != '\r' || it[1] != '\n')
            return false;
        it += 2;
        return true;
    }

    static
    string_view
    parse_method(char const*& it)
    {
        auto const first = it;
        while(detail::is_tchar(*it))
            ++it;
        return {first, static_cast<
            string_view::size_type>(
                it - first)};
    }

    static
    string_view
    parse_target(char const*& it)
    {
        auto const first = it;
        while(is_pathchar(*it))
            ++it;
        if(*it != ' ')
            return {};
        return {first, static_cast<
            string_view::size_type>(
                it - first)};
    }

    static
    string_view
    parse_name(char const*& it)
    {
        auto const first = it;
        while(to_field_char(*it))
            ++it;
        return {first, static_cast<
            string_view::size_type>(
                it - first)};
    }

    static
    int
    parse_version(char const*& it)
    {
        if(*it != 'H')
            return -1;
        if(*++it != 'T')
            return -1;
        if(*++it != 'T')
            return -1;
        if(*++it != 'P')
            return -1;
        if(*++it != '/')
            return -1;
        if(! is_digit(*++it))
            return -1;
        int v = 10 * (*it - '0');
        if(*++it != '.')
            return -1;
        if(! is_digit(*++it))
            return -1;
        v += *it++ - '0';
        return v;
    }

    static
    int
    parse_status(char const*& it)
    {
        int v;
        if(! is_digit(*it))
            return -1;
        v = 100 * (*it - '0');
        if(! is_digit(*++it))
            return -1;
        v += 10 * (*it - '0');
        if(! is_digit(*++it))
            return -1;
        v += (*it++ - '0');
        return v;
    }
    
    static
    string_view
    parse_reason(char const*& it)
    {
        auto const first = it;
        while(*it != '\r')
        {
            if(! is_text(*it))
                return {};
            ++it;
        }
        return {first, static_cast<
            std::size_t>(it - first)};
    }

    // VFALCO Can SIMD help this?
    static
    char const*
    find_eol(
        char const* it, char const* last,
            error_code& ec)
    {
#if 0
        // SLOWER
        it = reinterpret_cast<char const*>(
            std::memchr(it, '\r', last - it));
        if(! it)
        {
            ec.assign(0, ec.category());
            return nullptr;
        }
        if(it + 2 > last)
        {
            ec.assign(0, ec.category());
            return nullptr;
        }
        if(it[1] != '\n')
        {
            ec = error::bad_line_ending;
            return nullptr;
        }
        ec.assign(0, ec.category());
        return it + 2;
#else
        for(;;)
        {
            if(it == last)
            {
                ec.assign(0, ec.category());
                return nullptr;
            }
            if(*it == '\r')
            {
                if(++it == last)
                {
                    ec.assign(0, ec.category());
                    return nullptr;
                }
                if(*it != '\n')
                {
                    ec = error::bad_line_ending;
                    return nullptr;
                }
                ec.assign(0, ec.category());
                return ++it;
            }
            // VFALCO Should we handle the legacy case
            // for lines terminated with a single '\n'?
            ++it;
        }
#endif
    }

    // VFALCO Can SIMD help this?
    static
    char const*
    find_eom(char const* p, char const* last)
    {
        for(;;)
        {
            if(p + 4 > last)
                return nullptr;
            if(p[3] != '\n')
            {
                if(p[3] == '\r')
                    ++p;
                else
                    p += 4;
            }
            else if(p[2] != '\r')
            {
                p += 4;
            }
            else if(p[1] != '\n')
            {
                p += 2;
            }
            else if(p[0] != '\r')
            {
                p += 2;
            }
            else
            {
                return p + 4;
            }
        }
    }
};

} // detail
} // http
} // beast

#endif
