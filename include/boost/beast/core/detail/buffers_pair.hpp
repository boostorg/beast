//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_DETAIL_BUFFERS_PAIR_HPP
#define BOOST_BEAST_DETAIL_BUFFERS_PAIR_HPP

#include <boost/asio/buffer.hpp>
#include <boost/assert.hpp>
#include <type_traits>

namespace boost {
namespace beast {
namespace detail {

template<bool IsMutable>
class buffers_pair
{
    template<bool IsMutable_>
    friend class buffers_pair;

public:
    // VFALCO: This type is public otherwise
    //         asio::buffers_iterator won't compile.
    using value_type = typename
        std::conditional<IsMutable,
            net::mutable_buffer,
            net::const_buffer>::type;

    using const_iterator = value_type const*;

    buffers_pair() = default;
    buffers_pair(buffers_pair const&) = default;
    buffers_pair& operator=(buffers_pair const&) = default;

    template<
        bool IsMutable_ = IsMutable,
        class = typename
            std::enable_if<! IsMutable_>::type>
    buffers_pair(
        buffers_pair<true> const& other) noexcept
        : b_{other.b_[0], other.b_[1]}
    {
    }

    template<
        bool IsMutable_ = IsMutable,
        class = typename
        std::enable_if<! IsMutable_>::type>
    buffers_pair& operator=(
        buffers_pair<true> const& other) noexcept
    {
        b_ = {other.b_[0], other.b_[1]};
        return *this;
    }

    value_type&
    operator[](int i) noexcept
    {
        BOOST_ASSERT(i >= 0 && i < 2);
        return b_[i];
    }

    const_iterator
    begin() const noexcept
    {
        return &b_[0];
    }

    const_iterator
    end() const noexcept
    {
        if(b_[1].size() > 0)
            return &b_[2];
        return &b_[1];
    }
private:
    value_type b_[2];
};

} // detail
} // beast
} // boost

#endif
