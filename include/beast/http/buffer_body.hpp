//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_BUFFER_BODY_HPP
#define BEAST_HTTP_BUFFER_BODY_HPP

#include <beast/http/error.hpp>
#include <beast/http/message.hpp>
#include <beast/http/type_traits.hpp>
#include <boost/optional.hpp>
#include <type_traits>
#include <utility>

namespace beast {
namespace http {

/** A serializable body represented by caller provided buffers.

    This body type permits incremental message sending of caller
    provided buffers using a @ref serializer.

    @par Example
    @code
    template<class SyncWriteStream>
    void send(SyncWriteStream& stream)
    {
        ...
    }
    @endcode
*/
struct buffer_body
{
    /// The type of the body member when used in a message.
    struct value_type
    {
        /** A pointer to a contiguous area of memory of @ref size octets, else `nullptr`.

            If this is `nullptr` and @ref more is `true`, the error
            @ref error::need_buffer will be returned by a serializer
            when attempting to retrieve the next buffer.
        */
        void* data;

        /** The number of octets in the buffer pointed to by @ref data

            If @ref data is `nullptr` during serialization, this value
            is not inspected.
        */
        std::size_t size;

        /// `true` if this is not the last buffer.
        bool more;
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
        using is_deferred = std::false_type;

        using const_buffers_type =
            boost::asio::const_buffers_1;

        template<bool isRequest, class Fields>
        explicit
        reader(message<isRequest, buffer_body,
                Fields> const& msg)
            : body_(msg.body)
        {
        }

        void
        init(error_code&)
        {
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
                return boost::none;
            }
            if(body_.data)
            {
                toggle_ = true;
                return {{const_buffers_type{
                    body_.data, body_.size}, body_.more}};
            }
            if(body_.more)
                ec = error::need_buffer;
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
        writer(message<isRequest, buffer_body, Fields>& m)
            : body_(m.body)
        {
        }

        void
        init(boost::optional<std::uint64_t>, error_code&)
        {
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
            auto const bytes_transferred =
                buffer_copy(boost::asio::buffer(
                    body_.data, body_.size), buffers);
            body_.data = reinterpret_cast<char*>(
                body_.data) + bytes_transferred;
            body_.size -= bytes_transferred;
        }

        void
        finish(error_code&)
        {
        }
    };
#endif
};

#if ! BEAST_DOXYGEN
// operator<< is not supported for buffer_body"
template<bool isRequest, class Fields,
    bool isDeferred, class ConstBufferSequence>
std::ostream&
operator<<(std::ostream& os, message<isRequest,
    buffer_body, Fields> const& msg) = delete;
#endif

} // http
} // beast

#endif
