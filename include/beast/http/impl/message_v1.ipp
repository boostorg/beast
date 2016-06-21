//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_MESSAGE_V1_IPP
#define BEAST_HTTP_IMPL_MESSAGE_V1_IPP

#include <beast/core/error.hpp>
#include <beast/http/rfc7230.hpp>
#include <beast/http/detail/has_content_length.hpp>
#include <boost/optional.hpp>
#include <stdexcept>

namespace beast {
namespace http {

template<bool isRequest, class Body, class Headers>
bool
is_keep_alive(message_v1<isRequest, Body, Headers> const& msg)
{
    if(msg.version >= 11)
    {
        if(token_list{msg.headers["Connection"]}.exists("close"))
            return false;
        return true;
    }
    if(token_list{msg.headers["Connection"]}.exists("keep-alive"))
        return true;
    return false;
}

template<bool isRequest, class Body, class Headers>
bool
is_upgrade(message_v1<isRequest, Body, Headers> const& msg)
{
    if(msg.version < 11)
        return false;
    if(token_list{msg.headers["Connection"]}.exists("upgrade"))
        return true;
    return false;
}

namespace detail {

struct prepare_info
{
    boost::optional<connection> connection_value;
    boost::optional<std::uint64_t> content_length;
};

template<bool isRequest, class Body, class Headers>
inline
void
prepare_options(prepare_info& pi,
    message_v1<isRequest, Body, Headers>& msg)
{
}

template<bool isRequest, class Body, class Headers>
void
prepare_option(prepare_info& pi,
    message_v1<isRequest, Body, Headers>& msg,
        connection value)
{
    pi.connection_value = value;
}

template<
    bool isRequest, class Body, class Headers,
    class Opt, class... Opts>
void
prepare_options(prepare_info& pi,
    message_v1<isRequest, Body, Headers>& msg,
        Opt&& opt, Opts&&... opts)
{
    prepare_option(pi, msg, opt);
    prepare_options(pi, msg,
        std::forward<Opts>(opts)...);
}

template<bool isRequest, class Body, class Headers>
void
prepare_content_length(prepare_info& pi,
    message_v1<isRequest, Body, Headers> const& msg,
        std::true_type)
{
    typename Body::writer w(msg);
    // VFALCO This is a design problem!
    error_code ec;
    w.init(ec);
    if(ec)
        throw system_error{ec};
    pi.content_length = w.content_length();
}

template<bool isRequest, class Body, class Headers>
void
prepare_content_length(prepare_info& pi,
    message_v1<isRequest, Body, Headers> const& msg,
        std::false_type)
{
    pi.content_length = boost::none;
}

} // detail

template<
    bool isRequest, class Body, class Headers,
    class... Options>
void
prepare(message_v1<isRequest, Body, Headers>& msg,
    Options&&... options)
{
    // VFALCO TODO
    //static_assert(is_WritableBody<Body>::value,
    //  "WritableBody requirements not met");
    detail::prepare_info pi;
    detail::prepare_content_length(pi, msg,
        detail::has_content_length<typename Body::writer>{});
    detail::prepare_options(pi, msg,
        std::forward<Options>(options)...);

    if(msg.headers.exists("Connection"))
        throw std::invalid_argument(
            "prepare called with Connection field set");

    if(msg.headers.exists("Content-Length"))
        throw std::invalid_argument(
            "prepare called with Content-Length field set");

    if(token_list{msg.headers["Transfer-Encoding"]}.exists("chunked"))
        throw std::invalid_argument(
            "prepare called with Transfer-Encoding: chunked set");

    if(pi.connection_value != connection::upgrade)
    {
        if(pi.content_length)
        {
            // VFALCO TODO Use a static string here
            msg.headers.insert("Content-Length",
                std::to_string(*pi.content_length));
        }
        else if(msg.version >= 11)
        {
            msg.headers.insert("Transfer-Encoding", "chunked");
        }
    }

    auto const content_length =
        msg.headers.exists("Content-Length");

    if(pi.connection_value)
    {
        switch(*pi.connection_value)
        {
        case connection::upgrade:
            msg.headers.insert("Connection", "upgrade");
            break;

        case connection::keep_alive:
            if(msg.version < 11)
            {
                if(content_length)
                    msg.headers.insert("Connection", "keep-alive");
            }
            break;

        case connection::close:
            if(msg.version >= 11)
                msg.headers.insert("Connection", "close");
            break;
        }
    }

    // rfc7230 6.7.
    if(msg.version < 11 && token_list{
            msg.headers["Connection"]}.exists("upgrade"))
        throw std::invalid_argument(
            "invalid version for Connection: upgrade");
}

} // http
} // beast

#endif
