//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_STRING_BODY_HPP
#define BEAST_HTTP_STRING_BODY_HPP

#include <beast/config.hpp>
#include <beast/http/error.hpp>
#include <beast/http/message.hpp>
#include <beast/core/detail/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/optional.hpp>
#include <memory>
#include <string>
#include <utility>

namespace beast {
namespace http {

/** An HTTP message body represented by a `std::string`.

    Meets the requirements of @b Body.
*/
struct string_body
{
    /// The type of the body member when used in a message.
    using value_type = std::string;

    /// Returns the content length of the body in a message.
    template<bool isRequest, class Fields>
    static
    std::uint64_t
    size(
        message<isRequest, string_body, Fields> const& m)
    {
        return m.body.size();
    }

#if BEAST_DOXYGEN
    /// The algorithm to obtain buffers representing the body
    using reader = implementation_defined;
#else
    class reader
    {
        value_type const& body_;

    public:
        using is_deferred = std::false_type;

        using const_buffers_type =
            boost::asio::const_buffers_1;

        template<bool isRequest, class Fields>
        explicit
        reader(message<
                isRequest, string_body, Fields> const& msg)
            : body_(msg.body)
        {
        }

        void
        init(error_code&)
        {
        }

        boost::optional<std::pair<const_buffers_type, bool>>
        get(error_code&)
        {
            return {{const_buffers_type{
                body_.data(), body_.size()}, false}};
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
        writer(message<isRequest, string_body, Fields>& m)
            : body_(m.body)
        {
        }

        void
        init(boost::optional<
            std::uint64_t> content_length, error_code& ec)
        {
            if(content_length)
            {
                if(*content_length > (std::numeric_limits<
                        std::size_t>::max)())
                {
                    ec = make_error_code(
                        errc::not_enough_memory);
                    return;
                }
                body_.reserve(static_cast<
                    std::size_t>(*content_length));
            }
        }

        template<class ConstBufferSequence>
        void
        put(ConstBufferSequence const& buffers,
            error_code& ec)
        {
            using boost::asio::buffer_size;
            using boost::asio::buffer_copy;
            auto const n = buffer_size(buffers);
            auto const len = body_.size();
            try
            {
                body_.resize(len + n);
            }
            catch(std::length_error const&)
            {
                ec = error::buffer_overflow;
                return;
            }
            buffer_copy(boost::asio::buffer(
                &body_[0] + len, n), buffers);
        }

        void
        finish(error_code&)
        {
        }
    };
#endif
};

} // http
} // beast

#endif
