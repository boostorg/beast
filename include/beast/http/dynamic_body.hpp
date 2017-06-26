//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_DYNAMIC_BODY_HPP
#define BEAST_HTTP_DYNAMIC_BODY_HPP

#include <beast/config.hpp>
#include <beast/core/multi_buffer.hpp>
#include <beast/http/error.hpp>
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

    /// Returns the content length of this body in a message.
    static
    std::uint64_t
    size(value_type const& v)
    {
        return v.size();
    }

#if BEAST_DOXYGEN
    /// The algorithm to obtain buffers representing the body
    using reader = implementation_defined;
#else
    class reader
    {
        DynamicBuffer const& body_;

    public:
        using const_buffers_type =
            typename DynamicBuffer::const_buffers_type;

        template<bool isRequest, class Fields>
        explicit
        reader(message<isRequest, basic_dynamic_body,
                Fields> const& m, error_code& ec)
            : body_(m.body)
        {
            ec.assign(0, ec.category());
        }

        boost::optional<std::pair<const_buffers_type, bool>>
        get(error_code& ec)
        {
            ec.assign(0, ec.category());
            return {{body_.data(), false}};
        }
    };
#endif

#if BEAST_DOXYGEN
    /// The algorithm used store buffers in this body
    using writer = implementation_defined;
#else
    class writer
    {
        value_type& body_;

    public:
        template<bool isRequest, class Fields>
        explicit
        writer(message<isRequest, basic_dynamic_body, Fields>& msg,
            boost::optional<std::uint64_t> const&,
                error_code& ec)
            : body_(msg.body)
        {
            ec.assign(0, ec.category());
        }

        template<class ConstBufferSequence>
        void
        put(ConstBufferSequence const& buffers,
            error_code& ec)
        {
            using boost::asio::buffer_copy;
            using boost::asio::buffer_size;
            boost::optional<typename
                DynamicBuffer::mutable_buffers_type> b;
            try
            {
                b.emplace(body_.prepare(
                    buffer_size(buffers)));
            }
            catch(std::length_error const&)
            {
                ec = error::buffer_overflow;
                return;
            }
            ec.assign(0, ec.category());
            body_.commit(buffer_copy(*b, buffers));
        }

        void
        finish(error_code& ec)
        {
            ec.assign(0, ec.category());
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
