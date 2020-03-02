//
// Copyright (c) 2016-2020 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2020 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_ASIO_CORE_DETAIL_DYNAMIC_BUFFER_V0_HPP
#define BOOST_ASIO_CORE_DETAIL_DYNAMIC_BUFFER_V0_HPP

#include <boost/asio/buffer.hpp>
#include <type_traits>

namespace boost {
namespace beast {
namespace detail {

template<class Type, class Enable = void>
struct is_dynamic_buffer_v0 : std::false_type
{
};

template<
    class DynamicBuffer_v0,
    class MutableBuffersType = typename DynamicBuffer_v0::mutable_buffers_type,
    class ConstBuffersType = typename DynamicBuffer_v0::const_buffers_type>
struct v1_byref_dynamic_buffer_proxy;

template<class DynamicBuffer_v0>
struct v1_own_dynamic_buffer_proxy;

template<
    class DynamicBuffer_v0, class Enable = void>
struct select_dynamic_buffer_proxy;

template<class DynamicBuffer_v0>
struct select_dynamic_buffer_proxy<
    DynamicBuffer_v0,
    typename std::enable_if<
        is_dynamic_buffer_v0<DynamicBuffer_v0>::value
    >::type
>
{
    using type = v1_byref_dynamic_buffer_proxy<DynamicBuffer_v0>;
};

template<class DynamicBuffer_v1>
struct select_dynamic_buffer_proxy<
    DynamicBuffer_v1,
    typename std::enable_if<
        net::is_dynamic_buffer_v1<DynamicBuffer_v1>::value &&
        !is_dynamic_buffer_v0<DynamicBuffer_v1>::value
    >::type
>
{
    using type = v1_own_dynamic_buffer_proxy<DynamicBuffer_v1>;
};

// A class befriended by models of DynamicBuffer_v0 to allow access to the private
// v1 interface
// DynamicBuffer_v2 adapters can gain access to the v0 dynamic buffers they are adapting
// by using the static members of this class
struct dynamic_buffer_v2_access
{
    // Perform the v2 grow(n) operation on a v1 buffer
    template<class DynamicBuffer_v1>
    static
    void
    grow(DynamicBuffer_v1& db1, std::size_t n)
    {
        db1.commit(db1.prepare(n).size());
    }

    template<class DynamicBuffer_v0>
    static
    void
    shrink(DynamicBuffer_v0& db0, std::size_t n)
    {
        db0.shrink_impl(n);
    }

    template<class DynamicBuffer_v0>
    static
    typename DynamicBuffer_v0::mutable_buffers_type
    data(
        DynamicBuffer_v0& db0,
        std::size_t pos,
        std::size_t n)
    {
        return db0.data_impl(pos, n);
    }

    template<class DynamicBuffer_v0>
    static
    typename DynamicBuffer_v0::const_buffers_type
    data(
        DynamicBuffer_v0 const& db0,
        std::size_t pos,
        std::size_t n)
    {
        return db0.data_impl(pos, n);
    }

};

}
}
}

#endif
