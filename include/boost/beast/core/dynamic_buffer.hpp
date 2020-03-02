#ifndef BOOST_BEAST_CORE_DYNAMIC_BUFFER_HPP
#define BOOST_BEAST_CORE_DYNAMIC_BUFFER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/buffers_pair.hpp>
#include <boost/beast/core/detail/dynamic_buffer_v0.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/assert.hpp>
#include <boost/type_traits/make_void.hpp>

namespace boost {
namespace beast {

template<class DynamicBuffer_v0>
struct dynamic_buffer_v0_proxy
{
    BOOST_STATIC_ASSERT(detail::is_dynamic_buffer_v0<DynamicBuffer_v0>::value);

    dynamic_buffer_v0_proxy(DynamicBuffer_v0& storage)
        : storage_(storage)
    {
        boost::ignore_unused(storage_.prepare(0));
    }

    // DynamicBuffer_v2 interface

    using const_buffers_type = typename DynamicBuffer_v0::const_buffers_type;

    using mutable_buffers_type = typename DynamicBuffer_v0::mutable_buffers_type;

    std::size_t
    size() const
    {
        return storage_.size();
    }

    std::size_t
    max_size() const
    {
        return storage_.max_size();
    }

    std::size_t
    capacity() const
    {
        return storage_.capacity();
    }

    void
    consume(std::size_t n)
    {
        storage_.consume(n);
    }

    const_buffers_type
    data(std::size_t pos, std::size_t n) const
    {
        return detail::dynamic_buffer_v2_access::
        data(static_cast<DynamicBuffer_v0 const&>(storage_),
             pos, n);
    }

    mutable_buffers_type
    data(std::size_t pos, std::size_t n)
    {
        return detail::dynamic_buffer_v2_access::
        data(storage_, pos, n);
    }

    void grow(std::size_t n)
    {
        storage_.commit(net::buffer_size(storage_.prepare(n)));
    }

    void shrink(std::size_t n)
    {
        detail::dynamic_buffer_v2_access::
        shrink(storage_, n);
    }

private:
    DynamicBuffer_v0& storage_;
};

/**
 @brief Convert a reference to a v1 beast dynamic buffer into a copyable net.ts dynamic_buffer
 @details
     This function will automatically detect the type of dynamic buffer passes as an argument
     and will return a type modelling DynamicBuffer_v2 which will encapsulate, copy or reference
     the target as appropriate such that the returned object may be passed as an argument to
     any function which expects a model of DynamicBuffer_v2
 @tparam DynamicBuffer_v0
 @param target an lvalue reference to a model of ByRefV1DynamicBuffer
 @return an object which models a net.ts copyable dynamic buffer
 */
template<class DynamicBuffer_v0>
auto
dynamic_buffer(DynamicBuffer_v0& target)
-> //BOOST_BEAST_V1_DYNAMIC_BUFFER_PROXY(ByRefV1DynamicBuffer)
typename std::enable_if<
    detail::is_dynamic_buffer_v0<DynamicBuffer_v0>::value,
    dynamic_buffer_v0_proxy<DynamicBuffer_v0>>::type
{
    return
        dynamic_buffer_v0_proxy<
            DynamicBuffer_v0>(
                target);
}

template<class DynamicBuffer_v2>
auto
dynamic_buffer(DynamicBuffer_v2 buffer)
->
typename std::enable_if<
    boost::asio::is_dynamic_buffer_v2<DynamicBuffer_v2>::value,
    DynamicBuffer_v2>::type
{
    return buffer;
}

template<class Type, class = void>
struct convertible_to_dynamic_buffer_v2
: std::false_type
{
};

template<class Type>
struct convertible_to_dynamic_buffer_v2<
    Type,
    void_t<decltype(dynamic_buffer(std::declval<Type>()))>>
: std::true_type
{
};

}
}

#endif
