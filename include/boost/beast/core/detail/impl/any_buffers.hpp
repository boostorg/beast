//
// Copyright (c) 2016-2021 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

namespace boost {
namespace beast {
namespace detail {

template<bool IsConst>
template<class Buffers, class Alloc>
void any_buffers<IsConst>::impl<Buffers, Alloc>::destroy() noexcept
{
    auto bc = this->block_count;
    using allocator_type = typename
        std::allocator_traits<Alloc>::template
            rebind_alloc<impl>;
    auto alloc = allocator_type(a_);
    using atraits = std::allocator_traits<allocator_type>;
    atraits::destroy(alloc, this);

    using align_type = typename std::aligned_union<0, impl, value_type>::type;

    using align_alloc_type = typename
        std::allocator_traits<Alloc>::template
            rebind_alloc<align_type>;

    using align_alloc_traits =
        std::allocator_traits<align_alloc_type>;

    auto memory_allocator = align_alloc_type(a_);
    align_alloc_traits::deallocate(memory_allocator, reinterpret_cast<align_type*>(this), bc);
}

// ------------------------------------------------------------

template<bool IsConst>
template<class Buffers, class Alloc>
auto
any_buffers<IsConst>::construct(Buffers const& b, Alloc const& a)
-> any_buffers_impl_base<value_type>*
{

    using impl_type = impl<Buffers, Alloc>;

    // allocate memory for impl plus buffer array

    using align_type = typename std::aligned_union<0, impl_type, value_type>::type;

    using align_alloc_type = typename
        std::allocator_traits<Alloc>::template
            rebind_alloc<align_type>;

    using align_alloc_traits =
        std::allocator_traits<align_alloc_type>;

    auto memory_allocator = align_alloc_type(a);

    const auto buffer_count = std::distance(
        net::buffer_sequence_begin(b),
        net::buffer_sequence_end(b));
    const auto mem_required = sizeof(impl_type) + sizeof(value_type) * buffer_count;
    const auto blocks = (mem_required + sizeof(align_type) - 1) / sizeof(align_type);

    auto p = align_alloc_traits::allocate(memory_allocator, blocks);

    // construct impl (it will construct its own buffer array)

    using impl_allocator_type = typename
        std::allocator_traits<Alloc>::template
            rebind_alloc<impl_type>;
    auto impl_alloc = impl_allocator_type(a);
    using impl_traits = std::allocator_traits<impl_allocator_type>;

    try
    {
        impl_traits::construct(impl_alloc, static_cast<impl_type*>(static_cast<void*>(p)), b, a, blocks);
    }
    catch(...)
    {
        align_alloc_traits::deallocate(memory_allocator, p, blocks);
        throw;
    }
    return static_cast<any_buffers_impl_base<value_type>*>(reinterpret_cast<impl_type*>(p));
}

template<bool IsConst>
auto
any_buffers<IsConst>::begin() const noexcept
    -> value_type const*
{
    return impl_->buffers;
}

template<bool IsConst>
auto
any_buffers<IsConst>::end() const noexcept
    -> value_type const*
{
    return impl_->buffers + impl_->size;
}


}
}
}
