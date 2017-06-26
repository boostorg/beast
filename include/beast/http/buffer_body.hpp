//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_BUFFER_BODY_HPP
#define BEAST_HTTP_BUFFER_BODY_HPP

#include <beast/config.hpp>
#include <beast/http/error.hpp>
#include <beast/http/message.hpp>
#include <beast/http/type_traits.hpp>
#include <boost/optional.hpp>
#include <type_traits>
#include <utility>

namespace beast {
namespace http {

/** A serializable body represented by caller provided buffers.

    This body type permits the use of @ref parser or @ref serializer
    with caller provided buffers.
*/
struct buffer_body
{
    /// The type of the body member when used in a message.
    struct value_type
    {
        /** A pointer to a contiguous area of memory of @ref size octets, else `nullptr`.

            @par When Serializing

            If this is `nullptr` and `more` is `true`, the error
            @ref error::need_buffer will be returned from @ref serializer::get
            Otherwise, the serializer will use the memory pointed to
            by `data` having `size` octets of valid storage as the
            next buffer representing the body.

            @par When Parsing

            If this is `nullptr`, the error @ref error::need_buffer
            will be returned from @ref parser::put. Otherwise, the
            parser will store body octets into the memory pointed to
            by `data` having `size` octets of valid storage. After
            octets are stored, the `data` and `size` members are
            adjusted: `data` is incremented to point to the next
            octet after the data written, while `size` is decremented
            to reflect the remaining space at the memory location
            pointed to by `data`.
        */
        void* data = nullptr;

        /** The number of octets in the buffer pointed to by @ref data.

            @par When Serializing

            If `data` is `nullptr` during serialization, this value
            is ignored. Otherwise, it represents the number of valid
            body octets pointed to by `data`.

            @par When Parsing

            The value of this field will be decremented during parsing
            to indicate the number of remaining free octets in the
            buffer pointed to by `data`. When it reaches zero, the
            parser will return @ref error::need_buffer, indicating to
            the caller that the values of `data` and `size` should be
            updated to point to a new memory buffer.
        */
        std::size_t size = 0;

        /** `true` if this is not the last buffer.

            @par When Serializing
            
            If this is `true` and `data` is `nullptr`, the error
            @ref error::need_buffer will be returned from @ref serializer::get

            @par When Parsing

            This field is not used during parsing.
        */
        bool more = true;
    };

#if BEAST_DOXYGEN
    /// The algorithm to obtain buffers representing the body
    using reader = implementation_defined;
#else
    class reader
    {
        bool toggle_ = false;
        value_type const& body_;

    public:
        using const_buffers_type =
            boost::asio::const_buffers_1;

        template<bool isRequest, class Fields>
        explicit
        reader(message<isRequest, buffer_body,
                Fields> const& msg, error_code& ec)
            : body_(msg.body)
        {
            ec.assign(0, ec.category());
        }

        boost::optional<
            std::pair<const_buffers_type, bool>>
        get(error_code& ec)
        {
            if(toggle_)
            {
                if(body_.more)
                {
                    toggle_ = false;
                    ec = error::need_buffer;
                }
                else
                {
                    ec.assign(0, ec.category());
                }
                return boost::none;
            }
            if(body_.data)
            {
                ec.assign(0, ec.category());
                toggle_ = true;
                return {{const_buffers_type{
                    body_.data, body_.size}, body_.more}};
            }
            if(body_.more)
                ec = error::need_buffer;
            else
                ec.assign(0, ec.category());
            return boost::none;
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
        writer(message<isRequest, buffer_body, Fields>& m,
                boost::optional<std::uint64_t> const&,
                    error_code& ec)
            : body_(m.body)
        {
            ec.assign(0, ec.category());
        }

        template<class ConstBufferSequence>
        void
        put(ConstBufferSequence const& buffers,
            error_code& ec)
        {
            using boost::asio::buffer_size;
            using boost::asio::buffer_copy;
            auto const n = buffer_size(buffers);
            if(! body_.data || n > body_.size)
            {
                ec = error::need_buffer;
                return;
            }
            ec.assign(0, ec.category());
            auto const bytes_transferred =
                buffer_copy(boost::asio::buffer(
                    body_.data, body_.size), buffers);
            body_.data = reinterpret_cast<char*>(
                body_.data) + bytes_transferred;
            body_.size -= bytes_transferred;
        }

        void
        finish(error_code& ec)
        {
            ec.assign(0, ec.category());
        }
    };
#endif
};

#if ! BEAST_DOXYGEN
// operator<< is not supported for buffer_body
template<bool isRequest, class Fields>
std::ostream&
operator<<(std::ostream& os, message<isRequest,
    buffer_body, Fields> const& msg) = delete;
#endif

} // http
} // beast

#endif
