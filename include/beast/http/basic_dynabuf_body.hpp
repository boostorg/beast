//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_BASIC_DYNABUF_BODY_HPP
#define BEAST_HTTP_BASIC_DYNABUF_BODY_HPP

#include <beast/core/error.hpp>
#include <beast/http/message.hpp>
#include <beast/http/resume_context.hpp>
#include <beast/core/detail/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/optional.hpp>

namespace beast {
namespace http {

/** A message body represented by a @b `DynamicBuffer`

    Meets the requirements of @b `Body`.
*/
template<class DynamicBuffer>
struct basic_dynabuf_body
{
    /// The type of the `message::body` member
    using value_type = DynamicBuffer;

#if GENERATING_DOCS
private:
#endif

    class reader
    {
        value_type& body_;

        template<class F>
        static
        void
        tryf(error_code& ec, F&& f)
        {
            try
            {
                f();
            }
            catch(std::length_error const&)
            {
                ec = make_error_code(
                    boost::system::errc::message_size);
            }
            catch(std::bad_alloc const&)
            {
                ec = make_error_code(
                    boost::system::errc::not_enough_memory);
            }
            catch(std::exception const&)
            {
                ec = make_error_code(
                    boost::system::errc::io_error);
            }
        }

    public:
        using mutable_buffers_type =
            typename DynamicBuffer::mutable_buffers_type;

        template<bool isRequest, class Fields>
        explicit
        reader(message<isRequest,
                basic_dynabuf_body, Fields>& msg)
            : body_(msg.body)
        {
        }

        void
        init(boost::optional<
            std::uint64_t> const& content_length,
                error_code& ec)
        {
            beast::detail::ignore_unused(content_length);
            beast::detail::ignore_unused(ec);
        }

        boost::optional<mutable_buffers_type>
        prepare(std::size_t n, error_code& ec)
        {
            boost::optional<
                mutable_buffers_type> result;
            tryf(ec,
                [&]()
                {
                    result.emplace(
                        body_.prepare(n));
                });
            if(ec)
                return body_.prepare(0);
            return *result;
        }

        void
        commit(std::size_t n, error_code& ec)
        {
            tryf(ec,
                [&]()
                {
                    body_.commit(n);
                });
        }

        void
        finish(error_code& ec)
        {
            beast::detail::ignore_unused(ec);
        }
    };

    class writer
    {
        DynamicBuffer const& body_;

    public:
        template<bool isRequest, class Fields>
        explicit
        writer(message<
                isRequest, basic_dynabuf_body, Fields> const& m) noexcept
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
        boost::tribool
        write(resume_context&&, error_code&,
            WriteFunction&& wf) noexcept
        {
            wf(body_.data());
            return true;
        }
    };
};

} // http
} // beast

#endif
