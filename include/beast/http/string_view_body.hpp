//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_STRING_VIEW_BODY
#define BEAST_HTTP_STRING_VIEW_BODY

#include <beast/config.hpp>
#include <beast/core/string_view.hpp>
#include <beast/http/message.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/optional.hpp>
#include <utility>

namespace beast {
namespace http {

/** A readable HTTP message body represented by a @ref string_view.

    The application must ensure that the memory pointed to
    by the string view remains valid for the lifetime of
    any attempted operations.

    Meets the requirements of @b Body.
*/
struct string_view_body
{
    /// The type of the body member when used in a message.
    using value_type = string_view;

    /// Returns the content length of this body in a message.
    template<bool isRequest, class Fields>
    static
    std::uint64_t
    size(message<isRequest,
        string_view_body, Fields> const& m)
    {
        return m.body.size();
    }

#if BEAST_DOXYGEN
    /// The algorithm to obtain buffers representing the body
    using reader = implementation_defined;
#else
    class reader
    {
        string_view body_;

    public:
        using is_deferred = std::false_type;

        using const_buffers_type =
            boost::asio::const_buffers_1;

        template<bool isRequest, class Fields>
        explicit
        reader(message<
                isRequest, string_view_body, Fields> const& m)
            : body_(m.body)
        {
        }

        void
        init(error_code& ec)
        {
            ec = {};
        }

        boost::optional<std::pair<const_buffers_type, bool>>
        get(error_code& ec)
        {
            ec = {};
            return {{{body_.data(), body_.size()}, false}};
        }

        void
        finish(error_code& ec)
        {
            ec = {};
        }
    };
#endif
};

} // http
} // beast

#endif
