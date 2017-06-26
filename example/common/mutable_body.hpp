//
// Copyright (c) 2013-2017 Mike Gresens (mike dot gresens at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_EXAMPLE_COMMON_MUTABLE_BODY_HPP_
#define BEAST_EXAMPLE_COMMON_MUTABLE_BODY_HPP_

#include <beast/config.hpp>
#include <beast/http/error.hpp>
#include <beast/http/message.hpp>
#include <beast/core/detail/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/optional.hpp>
#include <boost/tti/has_type.hpp>
#include <boost/tti/has_member_function.hpp>
#include <boost/mpl/identity.hpp>
#include <memory>
#include <string>
#include <utility>

namespace mutable_detail
{
    BOOST_TTI_HAS_TYPE(value_type)
    BOOST_TTI_HAS_TYPE(iterator)
    BOOST_TTI_HAS_TYPE(size_type)
    BOOST_TTI_HAS_TYPE(reference)
    BOOST_TTI_HAS_MEMBER_FUNCTION(data)
    BOOST_TTI_HAS_MEMBER_FUNCTION(reserve)
    BOOST_TTI_HAS_MEMBER_FUNCTION(resize)

    template <typename T>
    using is_container = boost::mpl::bool_<
        has_type_value_type<T>::value &&
        has_type_iterator<T>::value &&
        has_type_size_type<T>::value &&
        has_type_reference<T>::value &&
        has_member_function_data<T, const typename T::value_type*, boost::mpl::vector<>, boost::function_types::const_qualified>::value &&
        has_member_function_reserve<T, void, boost::mpl::vector<size_t>>::value &&
        has_member_function_resize<T, void, boost::mpl::vector<size_t>>::value
    >;
}

/** An HTTP message body represented by a mutable character container.

    Meets the requirements of @b Body.
*/
template <typename Container>
struct mutable_body
{
    static_assert(sizeof(typename Container::value_type) == 1, "Mutable character requirements not met");
    static_assert(mutable_detail::is_container<Container>::value, "Mutable container requirements not met");

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
                isRequest, mutable_body, Fields> const& msg)
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
        writer(beast::http::message<isRequest, mutable_body, Fields>& m)
            : body_(m.body)
        {
        }

        void
        init(boost::optional<
            std::uint64_t> content_length, beast::error_code& ec)
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
