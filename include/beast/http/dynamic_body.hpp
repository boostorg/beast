//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_DYNAMIC_BODY_HPP
#define BEAST_HTTP_DYNAMIC_BODY_HPP

#include <beast/config.hpp>
#include <beast/core/error.hpp>
#include <beast/core/multi_buffer.hpp>
#include <beast/http/message.hpp>
#include <boost/optional.hpp>
#include <utility>

namespace beast {
namespace http {

/** An HTTP message body represented by a @b DynamicBuffer.

    Meets the requirements of @b Body.
*/
template<class DynamicBuffer>
struct basic_dynamic_body
{
    /// The type of the body member when used in a message.
    using value_type = DynamicBuffer;

#if BEAST_DOXYGEN
    /// The algorithm used when parsing this body.
    using reader = implementation_defined;
#else
    class reader
    {
        value_type& body_;

    public:
        static bool constexpr is_direct = true;

        using mutable_buffers_type =
            typename DynamicBuffer::mutable_buffers_type;

        template<bool isRequest, class Fields>
        explicit
        reader(message<isRequest,
                basic_dynamic_body, Fields>& msg)
            : body_(msg.body)
        {
        }

        void
        init()
        {
        }

        void
        init(std::uint64_t content_length)
        {
        }

        mutable_buffers_type
        prepare(std::size_t n)
        {
            return body_.prepare(n);
        }

        void
        commit(std::size_t n)
        {
            body_.commit(n);
        }

        void
        finish()
        {
        }
    };
#endif

#if BEAST_DOXYGEN
    /// The algorithm used when serializing this body.
    using writer = implementation_defined;
#else
    class writer
    {
        DynamicBuffer const& body_;

    public:
        using is_deferred = std::false_type;

        using const_buffers_type =
            typename DynamicBuffer::const_buffers_type;

        template<bool isRequest, class Fields>
        explicit
        writer(message<
                isRequest, basic_dynamic_body, Fields> const& m)
            : body_(m.body)
        {
        }

        void
        init(error_code&)
        {
        }

        std::uint64_t
        content_length() const
        {
            return body_.size();
        }

        boost::optional<std::pair<const_buffers_type, bool>>
        get(error_code& ec)
        {
            return {{body_.data(), false}};
        }
    };
#endif
};

/** A dynamic message body represented by a @ref multi_buffer

    Meets the requirements of @b Body.
*/
using dynamic_body = basic_dynamic_body<multi_buffer>;

} // http
} // beast

#endif
