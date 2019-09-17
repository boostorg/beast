//
// Copyright (c) 2018 jackarain (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef SOCKS_DETAIL_HOST_HPP
#define SOCKS_DETAIL_HOST_HPP

#include <socks/config.hpp>

namespace socks {
namespace detail {

enum host_type
{
	host_unkwon,
	host_ipv4,
	host_ipv6,
	host_domain,
	host_invalid,
};

inline
host_type
is_ipv4_host(string_view str)
{
    const char* b = str.data();
    const char* e = str.data() + str.size();
    int parts = 0;
    const char* start = b;
    int64_t last = 0;
    int64_t max = 0;

    while (b != e)
    {
        const char c = *b++;
        bool eol = b == e;

        if (c == '.' || eol)
        {
            if (++parts > 4)
                return host_unkwon;

            const char* end = eol ? b : b - 1;
            last = socks::detail::from_string(string_view(start, end - start));
            if (last < 0)
                return host_unkwon;

            if (max < last)
                max = last;

            start = b;
        }
    }

    if (parts == 0 || parts > 4)
    {
        if (str.size() == 0)
            return host_invalid;
        return host_unkwon;
    }

    if (max > 255 && last < max)
        return host_invalid;

    last >>= (8 * (4 - (parts - 1)));
    if (last != 0)
        return host_invalid;

    return host_ipv4;
}

inline
host_type
is_ipv6_host(string_view str)
{
    const char* b = str.data();
    const char* e = str.data() + str.size();
    const char* start = b;
    int parts = 0;
    int colons = 0;
    char last_char = '\0';
    uint16_t value[8];

    while (b != e)
    {
        const char c = *b++;
        bool eol = b == e;

        if (c == ':' || eol)
        {
            const char* end = eol ? b : b - 1;
            int64_t n = socks::detail::from_string(string_view(start, end - start), 16);
            if (n > 0xffff)
                return host_invalid;

            value[parts] = static_cast<uint16_t>(n);
            parts++;
            start = b;

            if (last_char == ':' && last_char == c)
            {
                colons++;
                if (colons > 1)
                    return host_invalid;
            }

            bool is_ipv4 = false;
            if (parts == 3 && colons == 1 && n == 0xffff) // ipv4
                is_ipv4 = true;

            if (parts == 6
                && colons == 0
                && (value[0] == 0 && value[1] == 0 && value[2] == 0
                    && value[3] == 0 && value[4] == 0)
                && n == 0xffff) // ipv4
                is_ipv4 = true;

            if (is_ipv4)
            {
                if (is_ipv4_host(string_view(b, e - b)) != host_ipv4)
                    return host_invalid;
                return host_ipv6;
            }
        }
        else
        {
            if (!socks::detail::ishexdigit(c))
                return host_invalid;
        }

        last_char = c;
    }

    if ((parts > 8) || (parts < 8 && colons == 0))
        return host_invalid;

    return host_ipv6;
}

} // detail
} // socks

#endif
