//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DETAIL_DYNAMIC_BUFFER_REF_HPP
#define BOOST_BEAST_CORE_DETAIL_DYNAMIC_BUFFER_REF_HPP

#include <boost/asio/buffer.hpp>
#include <cstdlib>
#include <type_traits>

namespace boost {
namespace beast {
namespace detail {

template<class DynamicBuffer>
class dynamic_buffer_ref
{
    DynamicBuffer& b_;

public:
    using const_buffers_type = typename
        DynamicBuffer::const_buffers_type;

    using mutable_buffers_type = typename
        DynamicBuffer::mutable_buffers_type;

    dynamic_buffer_ref(
        dynamic_buffer_ref&&) = default;

    explicit
    dynamic_buffer_ref(
        DynamicBuffer& b) noexcept
        : b_(b)
    {
    }

    std::size_t
    size() const noexcept
    {
        return b_.size();
    }

    std::size_t
    max_size() const noexcept
    {
        return b_.max_size();
    }

    std::size_t
    capacity() const noexcept
    {
        return b_.capacity();
    }

    const_buffers_type
    data() const noexcept
    {
        return b_.data();
    }

    mutable_buffers_type
    prepare(std::size_t n)
    {
        return b_.prepare(n);
    }

    void
    commit(std::size_t n)
    {
        b_.commit(n);
    }

    void
    consume(std::size_t n)
    {
        b_.consume(n);
    }
};

template<class DynamicBuffer>
typename std::enable_if<
    net::is_dynamic_buffer<DynamicBuffer>::value,
    dynamic_buffer_ref<DynamicBuffer>>::type
ref(DynamicBuffer& b)
{
    return dynamic_buffer_ref<DynamicBuffer>(b);
}

} // detail
} // beast
} // boost

#endif
