//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_BUFFER_BODY_HPP
#define BEAST_HTTP_BUFFER_BODY_HPP

#include <beast/http/type_traits.hpp>
#include <beast/http/error.hpp>
#include <beast/http/message.hpp>
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

    @tparam isDeferred A `bool` which, when set to `true`,
    indicates to the serialization implementation that it should
    send the entire HTTP Header before attempting to acquire
    buffers representing the body.

    @tparam ConstBufferSequence The type of buffer sequence
    stored in the body by the caller.
*/
template<
    bool isDeferred,
    class ConstBufferSequence>
struct buffer_body
{
    static_assert(is_const_buffer_sequence<ConstBufferSequence>::value,
        "ConstBufferSequence requirements not met");

    /** The type of the body member when used in a message.

        When engaged, the first element of the pair represents
        the current buffer sequence to be written.

        The second element of the pair indicates whether or not
        additional buffers will be available. A value of `false`
        indicates the end of the message body.

        If the buffer in the value is disengaged, and the second
        element of the pair is `true`, @ref serializer operations
        will return @ref http::error::need_more. This signals the
        calling code that a new buffer should be placed into the
        body, or that the caller should indicate that no more
        buffers will be available.
    */
    using value_type =
        std::pair<boost::optional<ConstBufferSequence>, bool>;

#if BEAST_DOXYGEN
    /// The algorithm to obtain buffers representing the body
    using reader = implementation_defined;
#else
    class reader
    {
        bool toggle_ = false;
        value_type const& body_;

    public:
        using is_deferred =
            std::integral_constant<bool, isDeferred>;

        using const_buffers_type = ConstBufferSequence;

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

        boost::optional<std::pair<ConstBufferSequence, bool>>
        get(error_code& ec)
        {
            if(toggle_)
            {
                if(body_.second)
                {
                    toggle_ = false;
                    ec = error::need_more;
                }
                return boost::none;
            }
            if(body_.first)
            {
                toggle_ = true;
                return {{*body_.first, body_.second}};
            }
            if(body_.second)
                ec = error::need_more;
            return boost::none;
        }
    };
#endif
};

} // http
} // beast

#endif
