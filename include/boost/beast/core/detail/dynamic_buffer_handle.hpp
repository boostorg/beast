//
// Copyright (c) 2016-2020 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2020 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DETAIL_DYNAMIC_BUFFER_HANDLE_HPP
#define BOOST_BEAST_CORE_DETAIL_DYNAMIC_BUFFER_HANDLE_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/beast/core/detail/is_beast_dynamic_buffer_v1.hpp>
#include <memory>
#include <boost/assert.hpp>

namespace boost {
namespace beast {
namespace detail {

// @brief Defines the concept of a dynamic_buffer_handle
//
// A dynamic_buffer_handle is a copyable, lightweight object which:
// * may or may not own the DynamicBuffer is was constructed with (depending on the type of the given buffer)
// * implements an asio dynamic_buffer version 1 interface
//
// The reason for this class is that there are currently 3 types of dynamic buffer:
// 1. asio version 1, in which dynamic buffers are movable and carry state. This causes problems
//    when implementing composed operations
// 2. asio version 2, in which dynamic buffers are copyable but carry no state. Supporting this
//    would require writing new code paths in beast
// 3. beast version 1, in which dynamic buffers are passed by reference and carry state.
//
// This handle type will always 'do the right thing' depending on the detected type of the source buffer.
// @tparam DynamicBuffer is the decayed type of the original dynamic buffer provided by the user
// @tparam is a type indicating which behaviour should be implemented in the current specialisation of
// dynamic_buffer_handle. It is designed to be detected through the templates:
//  - net::is_dynamic_buffer_v1
//  - net::is_dynamic_buffer_v2
//  - beast::is_beast_dynamic_buffer_v1
template<class DynamicBuffer, class Behaviour>
struct dynamic_buffer_handle;


template<class T>
struct is_dynamic_buffer_handle_test
{
    template<class...Us> static auto test(dynamic_buffer_handle<Us...>*) -> std::true_type;
    static auto test(...) -> std::false_type;

    using type = decltype(test((T*)0));
};

template<class T>
struct is_dynamic_buffer_handle : is_dynamic_buffer_handle_test<typename std::decay<T>::type>::type {};

// a meta-function who's result type indicates a behaviour flas
template<class DynamicBuffer, class = void>
struct dynamic_buffer_select_behaviour;

template<class DynamicBuffer>
using dynamic_buffer_select_behaviour_t =
    typename dynamic_buffer_select_behaviour<DynamicBuffer>::type;

// meta-function to select a dynamic_buffer_handle variant from a given DynamicBuffer and behaviour
// this is the general case
template<class DynamicBuffer, class Behaviour = dynamic_buffer_select_behaviour_t<DynamicBuffer>>
struct select_dynamic_buffer_variant
{
    using type =
        dynamic_buffer_handle<
            DynamicBuffer,
            dynamic_buffer_select_behaviour_t <DynamicBuffer>
        >;
};

template<class DynamicBuffer>
using dynamic_buffer_handle_t = typename select_dynamic_buffer_variant<typename std::decay<DynamicBuffer>::type>::type;

template<class SourceBuffer>
using reference_to_converted_dynamic_buffer_t = typename std::add_lvalue_reference<dynamic_buffer_handle_t<SourceBuffer>>::type;

// flag types indicating selected dynamic_buffer_handle implementation behaviour

// indicates that the implementation should treat the underlying buffer as:
// * move-only
// * asio dynamic_buffer version 1 interface
struct asio_v1_behaviour {};

// indicates that the implementation should treat the underlying buffer as:
// * copyable
// * asio dynamic_buffer version 2 interface
struct asio_v2_behaviour {};

// indicates that the implementation should treat the underlying buffer as:
// * non-moveable, non-copyable
// * asio dynamic_buffer version 1 interface
struct beast_v1_behaviour {};

struct dynamic_buffer_handle_behaviour {};

// specialise for recursive creation of dyamic_buffer_handle from another dynamic_buffer_handle

template<class Underlying>
struct select_dynamic_buffer_variant<Underlying, dynamic_buffer_handle_behaviour>
{
    using type = Underlying;
};

template<class DynamicBuffer>
struct
dynamic_buffer_select_behaviour
    <
        DynamicBuffer,
        typename std::enable_if
            <
                is_dynamic_buffer_handle<DynamicBuffer>::value
            >::type
    >
{
    using type = dynamic_buffer_handle_behaviour;
};

template<class...Ts, class Allocator = std::allocator<void>>
auto
make_dynamic_buffer_handle(
    dynamic_buffer_handle<Ts...> const & source,
    Allocator = Allocator())
-> dynamic_buffer_handle<Ts...>
{
    return source;
}

template<class...Ts, class Allocator = std::allocator<void>>
auto
make_dynamic_buffer_handle(
    dynamic_buffer_handle<Ts...> && source,
    Allocator = Allocator())
-> dynamic_buffer_handle<Ts...>&&
{
    return std::move(source);
}


// a base class designed to be used with CRTP which implements a complet dynamic_buffer_v1 interface
// when the derived class is holding some kind of pointer to an underlying dynamic buffer which also
// has a v1 interface
template<class V1BufferType, class Derived>
struct asio_dynamic_buffer_v1_interface
{
    using const_buffers_type = typename V1BufferType::const_buffers_type;
    using mutable_buffers_type = typename V1BufferType::mutable_buffers_type;

    std::size_t
    capacity() const
    {
        return static_cast<Derived const*>(this)->dyn_buf().capacity();
    }

    void commit(std::size_t n)
    {
        return static_cast<Derived*>(this)->dyn_buf().commit(n);
    }

    void consume(std::size_t n)
    {
        return static_cast<Derived*>(this)->dyn_buf().consume(n);
    }

    const_buffers_type data() const
    {
        return static_cast<Derived const*>(this)->dyn_buf().data();
    }

    std::size_t max_size() const
    {
        return static_cast<Derived const*>(this)->dyn_buf().max_size();
    }

    mutable_buffers_type prepare(std::size_t n)
    {
        return static_cast<Derived*>(this)->dyn_buf().prepare(n);
    }

    std::size_t size() const
    {
        return static_cast<Derived const*>(this)->dyn_buf().size();
    }
};

// specialise for holding an asio v1 dynamic buffer

template<class DynamicBuffer>
struct
dynamic_buffer_select_behaviour
<
    DynamicBuffer,
    typename std::enable_if
    <
        net::is_dynamic_buffer_v1<DynamicBuffer>::value &&
        !net::is_dynamic_buffer_v2<DynamicBuffer>::value &&
        !is_beast_dynamic_buffer_v1<DynamicBuffer>::value &&
        !is_dynamic_buffer_handle<DynamicBuffer>::value
    >::type
>
{
    using type = asio_v1_behaviour;
};

template<class AsioV1DynamicBuffer>
struct dynamic_buffer_handle<
    AsioV1DynamicBuffer,
    asio_v1_behaviour
>
: asio_dynamic_buffer_v1_interface<
    AsioV1DynamicBuffer,
    dynamic_buffer_handle<
        AsioV1DynamicBuffer,
        asio_v1_behaviour
    >
>
{
    template<class Allocator>
    dynamic_buffer_handle(Allocator alloc, AsioV1DynamicBuffer underlying)
    : impl_(
        allocate_impl(
            alloc,
            std::move(underlying))
        )
    {
        BOOST_ASSERT(impl_);
    }

    AsioV1DynamicBuffer& dyn_buf()
    {
        return *impl_;
    }

    AsioV1DynamicBuffer const & dyn_buf() const
    {
        return *impl_;
    }

private:

    using impl_class = AsioV1DynamicBuffer;

    using implementation_type = std::shared_ptr<impl_class>;

    template<class Allocator>
    static auto
    allocate_impl(Allocator alloc, AsioV1DynamicBuffer underlying)
    -> implementation_type
    {
        return std::allocate_shared<impl_class>(
            alloc,
            std::move(underlying));
    }

private:
    implementation_type impl_;
};

template<class AsioV1DynamicBuffer, class Allocator = std::allocator<void>>
auto
make_dynamic_buffer_handle(AsioV1DynamicBuffer underlying, Allocator alloc = Allocator())
->
typename std::enable_if
<
    std::is_same<
        dynamic_buffer_select_behaviour_t<AsioV1DynamicBuffer>,
        asio_v1_behaviour
    >::value,
    dynamic_buffer_handle_t<AsioV1DynamicBuffer>
>::type
{
    return dynamic_buffer_handle_t<AsioV1DynamicBuffer>(alloc, std::move(underlying));
}

// specialise for asio v2 buffers

template<class DynamicBuffer>
struct
dynamic_buffer_select_behaviour
    <
        DynamicBuffer,
        typename std::enable_if
            <
                net::is_dynamic_buffer_v2<DynamicBuffer>::value &&
                !is_beast_dynamic_buffer_v1<DynamicBuffer>::value &&
                !is_dynamic_buffer_handle<DynamicBuffer>::value
            >::type
    >
{
    using type = asio_v2_behaviour;
};

template<class AsioV2DynamicBuffer>
struct dynamic_buffer_handle<AsioV2DynamicBuffer, asio_v2_behaviour>
{
public:

    using const_buffers_type = typename AsioV2DynamicBuffer::const_buffers_type;
    using mutable_buffers_type = typename AsioV2DynamicBuffer::mutable_buffers_type;

    template<class Allocator>
    dynamic_buffer_handle(
        Allocator alloc,
        AsioV2DynamicBuffer underlying)
    : impl_(allocate_impl(
                alloc,
                std::move(underlying)))
    {}

    std::size_t
    capacity() const
    {
        return get_impl().dyn_buf.capacity();
    }

    void
    commit(std::size_t n)
    {
        auto& impl = get_impl();
        if (n < impl.prepared)
        {
            auto excess = impl.prepared - n;
            impl.dyn_buf.shrink(excess);
        }
        impl.prepared = 0;
    }

    void consume(std::size_t n)
    {
        get_impl().dyn_buf.consume(n);
    }

    const_buffers_type data() const
    {
        auto& impl = get_impl();
        return impl.dyn_buf.data(impl.dyn_buf.size() - impl.prepared, impl.prepared);
    }

    std::size_t max_size() const
    {
        return get_impl().dyn_buf.max_size();
    }

    mutable_buffers_type
    prepare(std::size_t n)
    {
        auto &impl = get_impl();
        if (impl.dyn_buf.size() + n > impl.dyn_buf.max_size())
            BOOST_THROW_EXCEPTION(std::length_error("prepare"));
        impl.prepared = n;
        impl.dyn_buf.grow(n);
        auto buffers =
            impl.dyn_buf.data(
                impl.dyn_buf.size() - impl.prepared,
                impl.prepared);
        return buffers;
    }

    std::size_t size() const
    {
        auto& impl = get_impl();
        return impl.dyn_buf.size() - impl.prepared;
    }

private:
    struct impl_class
    {
        impl_class(AsioV2DynamicBuffer dyn_buf_)
            : dyn_buf(std::move(dyn_buf_))
            , prepared(0)
        {}

        AsioV2DynamicBuffer dyn_buf; // a copy of the dynamic buffer
        std::size_t prepared; // the value of n in the last call to prepare(n)
    };

    using implementation_type = std::shared_ptr<impl_class>;

    template<class Allocator>
    static auto
    allocate_impl(Allocator alloc, AsioV2DynamicBuffer underlying)
    -> implementation_type
    {
        using atraits = std::allocator_traits<Allocator>;
        using our_allocator = typename atraits::template rebind_alloc<impl_class>;
        return std::allocate_shared<impl_class>(
            our_allocator(alloc),
            std::move(underlying));
    }

    auto get_impl() const -> impl_class const&
    {
        return *impl_;
    }

    auto get_impl() -> impl_class &
    {
        return *impl_;
    }

    implementation_type impl_;
};

template<class AsioV2DynamicBuffer, class Allocator = std::allocator<void>>
auto
make_dynamic_buffer_handle(
    AsioV2DynamicBuffer underlying,
    Allocator alloc = Allocator())
->
typename std::enable_if
<
    std::is_same<
        dynamic_buffer_select_behaviour_t<AsioV2DynamicBuffer>,
        asio_v2_behaviour
    >::value,
    dynamic_buffer_handle_t<AsioV2DynamicBuffer>
>::type
{
    return dynamic_buffer_handle_t<AsioV2DynamicBuffer>(
        std::move(alloc),
        std::move(underlying));
}

// specialise for beast v1 behaviour

template<class DynamicBuffer>
struct
dynamic_buffer_select_behaviour
    <
        DynamicBuffer,
        typename std::enable_if
            <
                net::is_dynamic_buffer_v1<DynamicBuffer>::value &&
                !net::is_dynamic_buffer_v2<DynamicBuffer>::value &&
                is_beast_dynamic_buffer_v1<DynamicBuffer>::value &&
                !is_dynamic_buffer_handle<DynamicBuffer>::value
            >::type
    >
{
    using type = beast_v1_behaviour;
};

template<class BeastV1DynamicBuffer>
struct dynamic_buffer_handle<BeastV1DynamicBuffer, beast_v1_behaviour>
: asio_dynamic_buffer_v1_interface<
    BeastV1DynamicBuffer,
    dynamic_buffer_handle<
        BeastV1DynamicBuffer,
        beast_v1_behaviour
    >
>
{
    dynamic_buffer_handle(BeastV1DynamicBuffer& underlying)
    : impl_(std::addressof(underlying))
    {}

    BeastV1DynamicBuffer &
    dyn_buf()
    {
        return *impl_;
    }

    BeastV1DynamicBuffer const &
    dyn_buf() const
    {
        return *impl_;
    }

private:
    using implementation_type = BeastV1DynamicBuffer *;
    implementation_type impl_;
};

template<class BeastV1DynamicBuffer, class Allocator = std::allocator<void>>
auto
make_dynamic_buffer_handle(BeastV1DynamicBuffer& underlying, Allocator = Allocator())
->
typename std::enable_if
    <
        std::is_same<
            dynamic_buffer_select_behaviour_t<BeastV1DynamicBuffer>,
            beast_v1_behaviour
        >::value,
        dynamic_buffer_handle_t<BeastV1DynamicBuffer>
    >::type
{
    return dynamic_buffer_handle_t<BeastV1DynamicBuffer>(underlying);
}

} // detail
} // beast
} // boost


#endif // BOOST_BEAST_CORE_DETAIL_DYNAMIC_BUFFER_HANDLE_HPP

