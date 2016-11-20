//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_STRING_BODY_HPP
#define BEAST_HTTP_STRING_BODY_HPP

#include <beast/core/error.hpp>
#include <beast/http/message.hpp>
#include <beast/http/resume_context.hpp>
#include <beast/core/detail/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/logic/tribool.hpp>
#include <memory>
#include <string>

namespace beast {
namespace http {

/** A Body represented by a std::string.

    Meets the requirements of @b `Body`.
*/
struct string_body
{
    /// The type of the `message::body` member
    using value_type = std::string;

#if GENERATING_DOCS
private:
#endif

    class reader
    {
        value_type& body_;
        std::size_t len_ = 0;

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
            boost::asio::mutable_buffers_1;

        template<bool isRequest, class Fields>
        explicit
        reader(message<isRequest,
                string_body, Fields>& m)
            : body_(m.body)
        {
        }

        void
        init(boost::optional<
            std::uint64_t> const& content_length,
                error_code& ec)
        {
            if(content_length)
            {
                if(*content_length >
                        (std::numeric_limits<std::size_t>::max)())
                {
                    ec = make_error_code(
                        boost::system::errc::message_size);
                    return;
                }
                tryf(ec,
                    [&]()
                    {
                        body_.reserve(*content_length);
                    });
            }
        }

        boost::optional<mutable_buffers_type>
        prepare(std::size_t n, error_code& ec)
        {
            tryf(ec,
                [&]()
                {
                    body_.resize(len_ + n);
                });
            return mutable_buffers_type{
                &body_[len_], n};
        }

        void
        commit(std::size_t n, error_code& ec)
        {
            if(body_.size() > len_ + n)
                tryf(ec,
                    [&]()
                    {
                        body_.resize(len_ + n);
                    });
            len_ = body_.size();
        }

        void
        finish(error_code&)
        {
        }
    };

    class writer
    {
        value_type const& body_;

    public:
        template<bool isRequest, class Fields>
        explicit
        writer(message<
                isRequest, string_body, Fields> const& msg) noexcept
            : body_(msg.body)
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
            wf(boost::asio::buffer(body_));
            return true;
        }
    };
};

} // http
} // beast

#endif
