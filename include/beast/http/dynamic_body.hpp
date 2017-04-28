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
#include <beast/core/streambuf.hpp>
#include <beast/http/message.hpp>

namespace beast {
namespace http {

/** A message body represented by a @b `DynamicBuffer`

    Meets the requirements of @b `Body`.
*/
template<class DynamicBuffer>
struct basic_dynamic_body
{
    /// The type of the `message::body` member
    using value_type = DynamicBuffer;

#if BEAST_DOXYGEN
private:
#endif

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

    class writer
    {
        DynamicBuffer const& body_;

    public:
        template<bool isRequest, class Fields>
        explicit
        writer(message<
                isRequest, basic_dynamic_body, Fields> const& m) noexcept
            : body_(m.body)
        {
        }

        void
        init(error_code& ec) noexcept
        {
            beast::detail::ignore_unused(ec);
        }

        std::uint64_t
        content_length() const noexcept
        {
            return body_.size();
        }

        template<class WriteFunction>
        bool
        write(error_code&, WriteFunction&& wf) noexcept
        {
            wf(body_.data());
            return true;
        }
    };
};

/** A dynamic message body represented by a @ref streambuf

    Meets the requirements of @b `Body`.
*/
using dynamic_body = basic_dynamic_body<streambuf>;

} // http
} // beast

#endif
