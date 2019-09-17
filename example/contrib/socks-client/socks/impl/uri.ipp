//
// Copyright (c) 2018 jackarain (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef SOCKS_IMPL_URI_IPP
#define SOCKS_IMPL_URI_IPP

#include <socks/detail/char_type.hpp>
#include <socks/detail/host.hpp>
#include <string>
#include <stdexcept>
#include <iterator>

namespace socks {

uri::
uri(const char* s)
{
    if (!parse(s))
        throw std::invalid_argument("URI malformed");
}

uri::
uri(const std::string& s)
{
    if (!parse(s))
        throw std::invalid_argument("URI malformed");
}

uri::
uri(string_view s)
{
    if (!parse(s))
        throw std::invalid_argument("URI malformed");
}

bool
uri::
parse(string_view url) noexcept
{
    using namespace detail;
    using boost::beast::iequals;

    enum
    {
        scheme_start,
        scheme,
        slash_start,
        slash,
        urn,
        probe_userinfo_hostname,
        host,
        port,
        path,
        query,
        fragment
    } state = scheme_start;

    auto b = url.data();
    auto e = url.data() + url.size();
    auto part_start = b;
    const char* v6_start = nullptr;
    const char* v6_end = nullptr;
    bool is_ipv6 = false;
    bool has_port = false;
    bool port_vaild = false;
    string_view probe;

    auto check_host = [&]() {
        if (is_ipv6)
        {
            if (is_ipv6_host(host_) == host_invalid)
                return false;
        }
        else
        {
            if (is_ipv4_host(host_) == host_invalid)
                return false;
        }

        return true;
    };

    while (b != e)
    {
        auto c = *b++;
        switch (state)
        {
        case scheme_start:
            if (isalpha(c))
            {
                state = scheme;
                continue;
            }
            return false;
        case scheme:
            if (isalpha(c) || isdigit(c) ||
                c == '+' || c == '-' || c == '.') // e.g: stratum+tcp://stratum.example.com
                continue;
            if (c == ':')
            {
                scheme_ = string_view(part_start, b - part_start - 1);
                state = slash_start;
                continue;
            }
            return false;
        case slash_start:
            if (c == '/')
            {
                state = slash;
            }
            else
            {
                state = urn;
                part_start = --b;
            }
            continue;
        case urn:
            if (b == e)
            {
                path_ = string_view(part_start, b - part_start);
                return true;
            }
            if (c == '?')
            {
                path_ = string_view(part_start, b - part_start - 1);
                part_start = b;
                state = query;
                continue;
            }
            continue;
        case slash:
            if (c == '/')
            {
                if (iequals(scheme_, "file"))
                {
                    if (*b == '/')
                    {
                        state = path;
                        part_start = b;
                        continue;
                    }
                }
                state = probe_userinfo_hostname;
                part_start = b;
                continue;
            }
            return false;
        case probe_userinfo_hostname:
            if (c == '[')
            {
                if (v6_start)
                    is_ipv6 = false;
                else
                {
                    is_ipv6 = true;
                    v6_start = b;
                }
            }
            if (c == ']')
            {
                if (is_ipv6 && !v6_end)
                {
                    v6_end = b - 1;
                    if (!probe.empty())
                        probe = string_view();
                }
                else
                    is_ipv6 = false;
            }
            if (c == '@')
            {
                if (!probe.empty())
                {
                    auto pwd = probe.data() + probe.size() + 1;
                    password_ = string_view(pwd, b - pwd - 1);
                    username_ = probe;
                }
                else
                {
                    username_ = string_view(part_start, b - part_start - 1); // username
                }

                v6_start = nullptr;
                v6_end = nullptr;

                is_ipv6 = false;

                part_start = b;
                state = host;
                continue;
            }

            if (c == ':')
            {
                if (b == e)
                {
                    auto host_size = b - part_start - 1;
                    if (host_size == 0)
                        return false;
                    host_ = string_view(part_start, host_size);
                    if (!check_host())
                        return false;
                    return true;
                }

                if (probe.empty())
                {
                    auto part_size = b - part_start - 1;
                    probe = string_view(part_start, part_size); // username or hostname
                    port_vaild = true;
                    has_port = true;
		    continue;
                }
            }

            if (c == '/' || c == '?' || c == '#')
            {
                if (is_ipv6)
                {
                    if (!v6_start || !v6_end || v6_start == v6_end)
                        return false;

                    host_ = string_view(v6_start, v6_end - v6_start); // hostname
                    if (!probe.empty()) // port
                    {
                        if (!port_vaild)
                            return false;

                        auto port_start = probe.data() + probe.size() + 1;
                        auto port_size = b - port_start - 1;

                        port_ = string_view(port_start, port_size);
                    }
                }
                else
                {
                    if (v6_start || v6_end)
                        return false;

                    if (has_port)
                    {
                        if (!port_vaild)
                            return false;

                        auto port_start = probe.data() + probe.size() + 1;
                        auto port_size = b - port_start - 1;

                        port_ = string_view(port_start, port_size);
                        host_ = probe;
                    }
                    else // hostname
                    {
                        auto host_size = b - part_start - 1;
                        if (host_size == 0)
                            return false;

                        host_ = string_view(part_start, host_size);
                    }
                }

                if (!check_host())
                    return false;

                if (c == '/')
                {
                    part_start = --b;
                    state = path;
                    continue;
                }
                if (c == '?')
                {
                    if (b == e)
                        return true;
                    part_start = b;
                    state = query;
                    continue;
                }
                if (c == '#')
                {
                    if (b == e)
                        return true;
                    part_start = b;
                    state = fragment;
                    continue;
                }
            }
            if (!isdigit(c))
                port_vaild = false;

            if (b == e)
            {
                if (is_ipv6)
                {
                    if (!v6_start || !v6_end || v6_start == v6_end)
                        return false;

                    host_ = string_view(v6_start, v6_end - v6_start); // hostname
                    if (!probe.empty()) // port
                    {
                        if (!port_vaild)
                            return false;

                        auto port_start = probe.data() + probe.size() + 1;
                        auto port_size = b - port_start;

                        port_ = string_view(port_start, port_size);
                    }
                }
                else
                {
                    if (v6_start || v6_end)
                        return false;

                    if (has_port)
                    {
                        if (!port_vaild)
                            return false;

                        auto port_start = probe.data() + probe.size() + 1;
                        auto port_size = b - port_start;

                        port_ = string_view(port_start, port_size);
                        host_ = probe; // hostname
                    }
                    else // hostname
                    {
                        auto host_size = b - part_start;
                        if (host_size == 0)
                            return false;

                        host_ = string_view(part_start, host_size);
                    }
                }

                if (!check_host())
                    return false;

                return true;
            }
            if (isunreserved(c) || issubdelims(c) || c == '%' || c == ' ' || c == '[' || c == ']' || c == ':')
                continue;
            return false;
        case host:
            if (c == '[')
            {
                if (v6_start)
                    return false;

                is_ipv6 = true;
                v6_start = b;
                continue;
            }
            if (is_ipv6)
            {
                if (c == ']')
                {
                    v6_end = b - 1;
                    auto host_size = v6_end - v6_start;
                    if (host_size == 0)
                        return false;
                    host_ = string_view(v6_start, host_size);
                    if (!check_host())
                        return false;
                    if (b == e)
                        return true;
                    if (*b == ':')
                    {
                        b++; // skip ':'
                        part_start = b;
                        state = port;
                        continue;
                    }
                    if (*b == '/')
                    {
                        part_start = b;
                        state = path;
                        continue;
                    }
                    if (*b == '?')
                    {
                        if (b == e)
                            return true;
                        part_start = ++b; // skip '?'
                        state = query;
                        continue;
                    }
                    if (*b == '#')
                    {
                        if (b == e)
                            return true;
                        part_start = ++b; // skip '#'
                        state = fragment;
                        continue;
                    }
                    return false;
                }
                else if (c == '/')
                {
                    return false;
                }
            }
            else
            {
                if (c == ':')
                {
                    auto host_size = b - part_start - 1;
                    if (host_size == 0)
                        return false;
                    host_ = string_view(part_start, host_size);
                    if (!check_host())
                        return false;
                    part_start = b;
                    state = port;

                    continue;
                }
            }
            if (c == '/')
            {
                auto host_size = b - part_start - 1;
                if (host_size == 0)
                    return false;
                host_ = string_view(part_start, host_size);
                if (!check_host())
                    return false;
                part_start = --b;
                state = path;

                continue;
            }
            if (c == '?')
            {
                auto host_size = b - part_start - 1;
                if (host_size == 0)
                    return false;
                host_ = string_view(part_start, host_size);
                if (!check_host())
                    return false;
                if (b == e)
                    return true;
                part_start = b;
                state = query;
                continue;
            }
            if (c == '#')
            {
                auto host_size = b - part_start - 1;
                if (host_size == 0)
                    return false;
                host_ = string_view(part_start, host_size);
                if (!check_host())
                    return false;
                if (b == e)
                    return true;
                part_start = b;
                state = fragment;
                continue;
            }
            if (b == e) // end
            {
                auto host_size = b - part_start;
                if (host_size == 0)
                    return false;
                host_ = string_view(part_start, host_size);
                if (!check_host())
                    return false;
                return true;
            }
            if (isunreserved(c) || issubdelims(c) || c == '%' || c == ':' || c == '@')
                continue;
            return false;
        case port:
            if (c == '/')
            {
                port_ = string_view(part_start, b - part_start - 1);
                part_start = --b;
                state = path;
                continue;
            }
            if (c == '?')
            {
                port_ = string_view(part_start, b - part_start - 1);
                if (b == e)
                    return true;
                part_start = b;
                state = query;
                continue;
            }
            if (c == '#')
            {
                port_ = string_view(part_start, b - part_start - 1);
                if (b == e)
                    return true;
                part_start = b;
                state = fragment;
                continue;
            }
            if (b == e) // no path
            {
                port_ = string_view(part_start, b - part_start);
                return true;
            }
            if (isdigit(c))
                continue;
            return false;
        case path:
            if (c == '?')
            {
                path_ = string_view(part_start, b - part_start - 1);
                if (b == e)
                    return true;
                part_start = b;
                state = query;
                continue;
            }
            if (c == '#')
            {
                path_ = string_view(part_start, b - part_start - 1);
                if (b == e)
                    return true;
                part_start = b;
                state = fragment;
                continue;
            }
            if (b == e)
            {
                path_ = string_view(part_start, b - part_start);
                return true;
            }
            if (isunreserved(c) || issubdelims(c) || c == '%' || c == '/' ||
                c == '&' || c == ':')
                continue;
            return false;
        case query:
            if (c == '#')
            {
                query_string_ = string_view(part_start, b - part_start - 1);
                if (b == e)
                    return true;
                part_start = b;
                state = fragment;
                continue;
            }
            if (b == e)
            {
                query_string_ = string_view(part_start, b - part_start);
                return true;
            }
            if (ishsegment(c) || issubdelims(c) || c == '/' || c == '?')
                continue;
            return false;
        case fragment:
            if (b == e)
            {
                fragment_ = string_view(part_start, b - part_start);
                return true;
            }
            if (ishsegment(c) || issubdelims(c) || c == '/' || c == '?')
                continue;
            return false;
        }
    }

    return false;
}

std::string
uri::
encodeURI(string_view str) noexcept
{
    using namespace detail;
    std::string result;

    for (const auto& c : str)
    {
        if (isalpha(c) || isdigit(c) || uri_mark(c) || uri_reserved(c))
        {
            result += c;
        }
        else
        {
            result += '%';
            result += std::string(to_hex((unsigned char)c));
        }
    }

    return result;
}

std::string
uri::
decodeURI(string_view str)
{
    using namespace detail;
    std::string result;

    auto start = str.cbegin();
    auto end = str.cend();

    for (; start != end; ++start)
    {
        char c = *start;
        if (c == '%')
        {
            auto first = std::next(start);
            if (first == end)
                throw std::invalid_argument("URI malformed");

            auto second = std::next(first);
            if (second == end)
                throw std::invalid_argument("URI malformed");

            if (isdigit(*first))
                c = *first - '0';
            else if (*first >= 'A' && *first <= 'F')
                c = *first - 'A' + 10;
            else if (*first >= 'a' && *first <= 'f')
                c = *first - 'a' + 10;
            else
                throw std::invalid_argument("URI malformed");

            c <<= 4;

            if (isdigit(*second))
                c += *second - '0';
            else if (*second >= 'A' && *second <= 'F')
                c += *second - 'A' + 10;
            else if (*second >= 'a' && *second <= 'f')
                c += *second - 'a' + 10;
            else
                throw std::invalid_argument("URI malformed");

            if (uri_reserved(c) || c == '#')
                c = *start;
            else
                std::advance(start, 2);
        }

        result += c;
    }

    return result;
}

std::string
uri::
encodeURIComponent(string_view str) noexcept
{
    using namespace ::socks::detail;
    std::string result;

    for (const auto& c : str)
    {
        if (isalpha(c) || isdigit(c) || uri_mark(c))
        {
            result += c;
        }
        else
        {
            result += '%';
            result += std::string(to_hex((unsigned char)c));
        }
    }

    return result;
}

std::string
uri::
decodeURIComponent(string_view str)
{
    using namespace detail;
    std::string result;

    auto start = str.cbegin();
    auto end = str.cend();

    for (; start != end; ++start)
    {
        char c = *start;
        if (c == '%')
        {
            auto first = std::next(start);
            if (first == end)
                throw std::invalid_argument("URI malformed");

            auto second = std::next(first);
            if (second == end)
                throw std::invalid_argument("URI malformed");

            if (isdigit(*first))
                c = *first - '0';
            else if (*first >= 'A' && *first <= 'F')
                c = *first - 'A' + 10;
            else if (*first >= 'a' && *first <= 'f')
                c = *first - 'a' + 10;
            else
                throw std::invalid_argument("URI malformed");

            c <<= 4;

            if (isdigit(*second))
                c += *second - '0';
            else if (*second >= 'A' && *second <= 'F')
                c += *second - 'A' + 10;
            else if (*second >= 'a' && *second <= 'f')
                c += *second - 'a' + 10;
            else
                throw std::invalid_argument("URI malformed");

            std::advance(start, 2);
        }

        result += c;
    }

    return result;
}

string_view
uri::
known_port() noexcept
{
    using boost::beast::iequals;
    if (iequals(scheme_, "ftp"))
        return string_view("21");
    else if (iequals(scheme_, "ssh"))
        return string_view("22");
    else if (iequals(scheme_, "telnet"))
        return string_view("23");
    else if (iequals(scheme_, "gopher"))
        return string_view("70");
    else if (iequals(scheme_, "http") || iequals(scheme_, "ws"))
        return string_view("80");
    else if (iequals(scheme_, "nntp"))
        return string_view("119");
    else if (iequals(scheme_, "ldap"))
        return string_view("389");
    else if (iequals(scheme_, "https") || iequals(scheme_, "wss"))
        return string_view("443");
    else if (iequals(scheme_, "rtsp"))
        return string_view("554");
    else if (iequals(scheme_, "socks") || iequals(scheme_, "socks4") || iequals(scheme_, "socks5"))
        return string_view("1080");
    else if (iequals(scheme_, "sip"))
        return string_view("5060");
    else if (iequals(scheme_, "sips"))
        return string_view("5061");
    else if (iequals(scheme_, "xmpp"))
        return string_view("5222");
    return string_view("0");
}

} // socks

#endif
