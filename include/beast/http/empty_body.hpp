//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_EMPTY_BODY_HPP
#define BEAST_HTTP_EMPTY_BODY_HPP

#include <beast/config.hpp>
#include <beast/http/error.hpp>
#include <beast/http/message.hpp>
#include <boost/optional.hpp>

namespace beast {
namespace http {

/** An empty message body.

    This body is used to represent messages which do not have a
    message body. If this body is used with a parser, and the
    parser encounters octets corresponding to a message body,
    the parser will fail with the error @ref http::unexpected_body.

    Meets the requirements of @b Body. The Content-Length of this
    body is always 0.
*/
struct empty_body
{
    /// The type of the body member when used in a message.
    struct value_type
    {
        // VFALCO We could stash boost::optional<std::uint64_t>
        //        for the content length here, set on init()
    };

    /// Returns the content length of the body in a message.
    static
    std::uint64_t
    size(value_type)
    {
        return 0;
    }

#if BEAST_DOXYGEN
    /// The algorithm to obtain buffers representing the body
    using reader = implementation_defined;
#else
    struct reader
    {
        using const_buffers_type =
            boost::asio::null_buffers;

        template<bool isRequest, class Fields>
        explicit
        reader(message<isRequest, empty_body,
            Fields> const&, error_code& ec)
        {
            ec.assign(0, ec.category());
        }

        boost::optional<std::pair<const_buffers_type, bool>>
        get(error_code& ec)
        {
            ec.assign(0, ec.category());
            return boost::none;
        }
    };
#endif

#if BEAST_DOXYGEN
    /// The algorithm used store buffers in this body
    using writer = implementation_defined;
#else
    struct writer
    {
        template<bool isRequest, class Fields>
        explicit
        writer(message<isRequest, empty_body, Fields>&,
            boost::optional<std::uint64_t> const&,
                error_code& ec)
        {
            ec.assign(0, ec.category());
        }

        template<class ConstBufferSequence>
        void
        put(ConstBufferSequence const&,
            error_code& ec)
        {
            ec = error::unexpected_body;
        }

        void
        finish(error_code& ec)
        {
            ec.assign(0, ec.category());
        }
    };
#endif
};

} // http
} // beast

#endif
