//
// Copyright (c) 2013-2017 Mike Gresens (mike dot gresens at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_EXAMPLE_COMMON_CONST_BODY_HPP_
#define BEAST_EXAMPLE_COMMON_CONST_BODY_HPP_

#include <beast/config.hpp>
#include <beast/http/error.hpp>
#include <beast/http/message.hpp>
#include <beast/core/detail/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/optional.hpp>
#include <memory>
#include <string>
#include <utility>

namespace detail {

template<class T, class = void>
struct is_const_container : std::false_type { };

template< class T >
struct is_const_container<T, beast::detail::void_t<
    decltype( std::declval<T&>().size() ),
    decltype( std::declval<T&>().data() )
> > : std::true_type { };

}

/** An HTTP message body represented by a constant character container.

    Meets the requirements of @b Body.
*/
template <typename Container>
struct const_body
{
    static_assert(sizeof(typename Container::value_type) == 1, "Const character requirements not met");
    static_assert(detail::is_const_container<Container>::value, "Const container requirements2 not met");

    /// The type of the body member when used in a message.
    using value_type = Container;

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
        using is_deferred = std::true_type;

        using const_buffers_type =
            boost::asio::const_buffers_1;

        template<bool isRequest, class Fields>
        explicit
        reader(beast::http::message<
                isRequest, const_body, Fields> const& msg)
            : body_(msg.body)
        {
        }

        void
        init(beast::error_code& ec)
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

        void
        finish(beast::error_code& ec)
        {
            ec.assign(0, ec.category());
        }
    };
#endif
};

#endif
