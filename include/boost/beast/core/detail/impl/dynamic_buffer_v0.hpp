//
// Copyright (c) 2020 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_ASIO_CORE_DETAIL__IMPL_DYNAMIC_BUFFER_V0_HPP
#define BOOST_ASIO_CORE_DETAIL__IMPL_DYNAMIC_BUFFER_V0_HPP

#include <boost/beast/core/buffer_traits.hpp>

namespace boost {
namespace beast {
namespace detail {

template<class DynamicBuffer_v0>
dynamic_buffer_v0_proxy<DynamicBuffer_v0>::
dynamic_buffer_v0_proxy(
    DynamicBuffer_v0& storage) noexcept
    : storage_(std::addressof(storage))
{
    storage_->prepare(0);
}

template<class DynamicBuffer_v0>
auto
dynamic_buffer_v0_proxy<DynamicBuffer_v0>::
size() const ->
std::size_t
{
    return storage_->size();
}

template<class DynamicBuffer_v0>
auto
dynamic_buffer_v0_proxy<DynamicBuffer_v0>::
max_size() const ->
std::size_t
{
    return storage_->max_size();
}

template<class DynamicBuffer_v0>
auto
dynamic_buffer_v0_proxy<DynamicBuffer_v0>::
capacity() const ->
std::size_t
{
    return storage_->capacity();
}

template<class DynamicBuffer_v0>
auto
dynamic_buffer_v0_proxy<DynamicBuffer_v0>::
consume(std::size_t n) ->
void
{
    storage_->consume(n);
}

template<class DynamicBuffer_v0>
auto
dynamic_buffer_v0_proxy<DynamicBuffer_v0>::
data(std::size_t pos, std::size_t n) const ->
const_buffers_type
{
    return
        detail::dynamic_buffer_v2_access::
            data(static_cast<DynamicBuffer_v0 const&>(*storage_),
                 pos, n);
}

template<class DynamicBuffer_v0>
auto
dynamic_buffer_v0_proxy<DynamicBuffer_v0>::
data(std::size_t pos, std::size_t n) ->
mutable_buffers_type
{
    return
        detail::dynamic_buffer_v2_access::
            data(*storage_, pos, n);
}

template<class DynamicBuffer_v0>
auto
dynamic_buffer_v0_proxy<DynamicBuffer_v0>::
grow(std::size_t n) ->
void
{
    storage_->commit(
        buffer_bytes(
            storage_->prepare(n)));
}

template<class DynamicBuffer_v0>
auto
dynamic_buffer_v0_proxy<DynamicBuffer_v0>::
shrink(std::size_t n) ->
void
{
    detail::dynamic_buffer_v2_access::
    shrink(*storage_, n);
}

// ---------------------------------------------------------

template<class DynamicBuffer_v0>
auto
impl_dynamic_buffer(DynamicBuffer_v0& target) ->
typename std::enable_if<
    detail::is_dynamic_buffer_v0<DynamicBuffer_v0>::value,
    detail::dynamic_buffer_v0_proxy<DynamicBuffer_v0>>::type
{
    return
        detail::dynamic_buffer_v0_proxy<
            DynamicBuffer_v0>(
            target);
}

template<class Type, class = void>
struct convertible_to_dynamic_buffer_v2
    : std::false_type
{
};

template<class Type>
struct convertible_to_dynamic_buffer_v2<
    Type,
    void_t<decltype(impl_dynamic_buffer(std::declval<Type>()))>>
: std::true_type
{
};


} // detail
} // beast
} // boost

#endif // BOOST_ASIO_CORE_DETAIL__IMPL_DYNAMIC_BUFFER_V0_HPP
