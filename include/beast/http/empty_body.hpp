//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_EMPTY_BODY_HPP
#define BEAST_HTTP_EMPTY_BODY_HPP

#include <beast/config.hpp>
#include <beast/http/message.hpp>
#include <boost/optional.hpp>

namespace beast {
namespace http {

/** An empty message body

    Meets the requirements of @b Body.

    @note This body type may only be written, not read.
*/
struct empty_body
{
    /// The type of the body member when used in a message.
    struct value_type
    {
    };

#if BEAST_DOXYGEN
    /// The algorithm to obtain buffers representing the body
    using reader = implementation_defined;
#else
    struct reader
    {
        using is_deferred = std::false_type;

        using const_buffers_type =
            boost::asio::null_buffers;

        template<bool isRequest, class Fields>
        explicit
        reader(message<
            isRequest, empty_body, Fields> const&)
        {
        }

        void
        init(error_code&)
        {
        }

        std::uint64_t
        content_length() const
        {
            return 0;
        }

        boost::optional<std::pair<const_buffers_type, bool>>
        get(error_code& ec)
        {
            return boost::none;
        }
    };
#endif
};

} // http
} // beast

#endif
