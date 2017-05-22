//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_STRING_BODY_HPP
#define BEAST_HTTP_STRING_BODY_HPP

#include <beast/config.hpp>
#include <beast/core/error.hpp>
#include <beast/http/message.hpp>
#include <beast/core/detail/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <memory>
#include <string>

namespace beast {
namespace http {

/** A Body represented by a std::string.

    Meets the requirements of @b Body.
*/
struct string_body
{
    /// The type of the `message::body` member
    using value_type = std::string;

#if BEAST_DOXYGEN
private:
#endif

    class reader
    {
        value_type& body_;
        std::size_t len_ = 0;

    public:
        static bool constexpr is_direct = true;

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
        init()
        {
        }

        void
        init(std::uint64_t content_length)
        {
            if(content_length >
                    (std::numeric_limits<std::size_t>::max)())
                BOOST_THROW_EXCEPTION(std::length_error{
                    "Content-Length overflow"});
            body_.reserve(static_cast<
                std::size_t>(content_length));
        }

        mutable_buffers_type
        prepare(std::size_t n)
        {
            body_.resize(len_ + n);
            return {&body_[len_], n};
        }

        void
        commit(std::size_t n)
        {
            if(body_.size() > len_ + n)
                body_.resize(len_ + n);
            len_ = body_.size();
        }

        void
        finish()
        {
            body_.resize(len_);
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
        bool
        write(error_code&, WriteFunction&& wf) noexcept
        {
            wf(boost::asio::buffer(body_));
            return true;
        }
    };
};

} // http
} // beast

#endif
