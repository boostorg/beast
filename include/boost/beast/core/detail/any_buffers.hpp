//
// Copyright (c) 2016-2021 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DETAIL_ANY_BUFFERS_HPP
#define BOOST_BEAST_CORE_DETAIL_ANY_BUFFERS_HPP

#include <boost/beast/core/detail/config.hpp>

#include <boost/asio/buffer.hpp>

#include <atomic>
#include <cstddef>
#include <memory>
#include <type_traits>

namespace boost {
namespace beast {
namespace detail {

template<class ValueType>
class any_buffers_impl_base
{
public:
    any_buffers_impl_base(std::size_t bc)
    : block_count(bc)
    {}

    std::size_t const block_count;
    std::atomic<std::size_t> refs = { 1 };
    std::size_t size = 0;
    ValueType const* buffers = nullptr;

    std::size_t
    addref() noexcept;

    std::size_t
    release() noexcept;

    virtual ~any_buffers_impl_base() = default;

private:
    virtual
    void
    destroy() noexcept = 0;

};

template<bool IsConst>
class any_buffers
{
public:
    using value_type =
        typename std::conditional<
            IsConst,
            net::const_buffer,
            net::mutable_buffer>::type;

private:

    template<
        class Buffers,
        class Alloc = std::allocator<void>>
    struct alignas(value_type)
        impl : any_buffers_impl_base<value_type>
    {
        impl(
            Buffers const& b,
            Alloc const& a,
            std::size_t block_count)
        : any_buffers_impl_base<value_type>(block_count)
        , b_(b)
        , a_(a)
        {
            value_type* dest = reinterpret_cast<value_type*>(this + 1);
            this->buffers = dest;

            const auto last = net::buffer_sequence_end(b_);
            for(auto first = net::buffer_sequence_begin(b_)
                ; first != last
                ; ++first)
            {
                try
                {
                    new (dest++) value_type (*first);
                }
                catch(...)
                {
                    unwind();
                    throw;
                }
                ++this->size;
            }
        }

        ~impl()
        {
            unwind();
        }

        Buffers b_;
        Alloc   a_;

    private:
        void
        destroy() noexcept override;

        void unwind() noexcept
        {
            value_type const* p = this->buffers + this->size;
            while (p != this->buffers)
            {
                --p;
                p->~value_type();
            }
        }
    };

    any_buffers_impl_base<value_type>* impl_;

public:

    template<
        class Buffers,
        class Alloc = std::allocator<void>, typename std::enable_if<
            !std::is_base_of<any_buffers, Buffers>::value >::type* = nullptr>
    explicit
    any_buffers(
        Buffers const& b,
        Alloc const& a = {})
    : impl_(construct(b, a))
    {
    }

    template<bool const_ = IsConst, typename std::enable_if<const_>::type* = nullptr>
    any_buffers(any_buffers<false> const& other);

    BOOST_BEAST_DECL
    any_buffers(any_buffers const& r);

    BOOST_BEAST_DECL
    any_buffers(any_buffers && r) noexcept;

    any_buffers&
    operator=(any_buffers const& r) noexcept
    {
        if(impl_)
            impl_->release();

        impl_ = r.impl_;

        if(impl_)
            impl_->addref();

        return *this;
    }

    any_buffers&
    operator=(any_buffers && r) noexcept
    {
        if(impl_)
            impl_->release();

        impl_ = boost::exchange(r.impl_, nullptr);

        return *this;
    }

    BOOST_BEAST_DECL
    ~any_buffers();

    value_type const*
    begin() const noexcept;

    value_type const*
    end() const noexcept;

private:
    template<class Buffers, class Alloc>
    static
    any_buffers_impl_base<value_type>*
    construct(Buffers const& b, Alloc const& a);
};

using any_const_buffers = any_buffers<true>;
using any_mutable_buffers = any_buffers<false>;

template<class MutableBufferSequence, class Alloc = std::allocator<void>>
auto make_any_buffers(MutableBufferSequence const& buffers, Alloc const& alloc = Alloc())
-> typename std::enable_if<
    net::is_mutable_buffer_sequence<MutableBufferSequence>::value,
    any_mutable_buffers>::type
{
    return any_mutable_buffers(buffers, alloc);
}

template<class ConstBufferSequence, class Alloc = std::allocator<void>>
auto make_any_buffers(ConstBufferSequence const& buffers, Alloc const& alloc = Alloc())
-> typename std::enable_if<
    net::is_mutable_buffer_sequence<ConstBufferSequence>::value,
    any_const_buffers>::type
{
    return any_const_buffers(buffers, alloc);
}

} // detail
} // beast
} // boost

#include <boost/beast/core/detail/impl/any_buffers.hpp>

#endif // BOOST_BEAST_CORE_DETAIL_ANY_BUFFERS_HPP

#ifdef BOOST_BEAST_HEADER_ONLY
#include <boost/beast/core/detail/impl/any_buffers.ipp>
#endif
