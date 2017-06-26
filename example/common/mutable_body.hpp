//
// Copyright (c) 2017 Mike Gresens (mike dot gresens at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_EXAMPLE_COMMON_MUTABLE_BODY_HPP
#define BEAST_EXAMPLE_COMMON_MUTABLE_BODY_HPP

#include <beast/http/message.hpp>
#include <beast/http/error.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/optional.hpp>

namespace detail {

template <typename T>
using is_mutable_character = std::integral_constant<bool,
    std::is_integral<T>::value &&
    sizeof(T) == 1
>;

template<class T, class = void>
struct is_mutable_container : std::false_type { };

template< class T >
struct is_mutable_container<T, beast::detail::void_t<
    decltype( std::declval<typename T::size_type&>() = std::declval<T&>().size() ),
    decltype( std::declval<const typename T::value_type*&>() = std::declval<T&>().data() ),
    decltype( std::declval<T&>().reserve(0) ),
    decltype( std::declval<T&>().resize(0) )
> > : std::true_type { };

}

/** An HTTP message body represented by a mutable character container.

    Meets the requirements of @b Body.
*/
template <typename MutableContainer>
struct mutable_body
{
    static_assert(
        detail::is_mutable_character<typename MutableContainer::value_type>::value &&
        detail::is_mutable_container<MutableContainer>::value,
        "MutableContainer requirements not met");

    /// The type of the body member when used in a message.
    using value_type = MutableContainer;

    /// Returns the content length of the body in a message.
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
        value_type const& body_;

    public:
        using const_buffers_type =
            boost::asio::const_buffers_1;

        template<bool isRequest, class Fields>
        explicit
        reader(beast::http::message<isRequest, mutable_body,
                Fields> const& msg, beast::error_code& ec)
            : body_(msg.body)
        {
            ec.assign(0, ec.category());
        }

        boost::optional<std::pair<const_buffers_type, bool>>
        get(beast::error_code& ec)
        {
            ec.assign(0, ec.category());
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
        writer(beast::http::message<isRequest, mutable_body, Fields>& m,
            boost::optional<std::uint64_t> const& content_length,
                beast::error_code& ec)
            : body_(m.body)
        {
            if(content_length)
            {
                if(*content_length > (std::numeric_limits<
                        std::size_t>::max)())
                {
                    ec = boost::system::errc::make_error_code(
                            boost::system::errc::not_enough_memory);
                    return;
                }
                ec.assign(0, ec.category());
                body_.reserve(static_cast<
                    std::size_t>(*content_length));
            }
        }

        template<class ConstBufferSequence>
        void
        put(ConstBufferSequence const& buffers,
                beast::error_code& ec)
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
                ec = beast::http::error::buffer_overflow;
                return;
            }
            ec.assign(0, ec.category());
            buffer_copy(boost::asio::buffer(
                &body_[0] + len, n), buffers);
        }

        void
        finish(beast::error_code& ec)
        {
            ec.assign(0, ec.category());
        }
    };
#endif

};

#endif
