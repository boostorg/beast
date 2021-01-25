//
// Copyright (c) 2016-2021 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DETAIL_ANY_DYNAMIC_BUFFER_V0_REF_HPP
#define BOOST_BEAST_CORE_DETAIL_ANY_DYNAMIC_BUFFER_V0_REF_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/any_buffers.hpp>
#include <type_traits>

namespace boost {
namespace beast {
namespace detail {

/// @brief Wraps a reference to any
class any_dynamic_buffer_v0_ref
{
public:
    template<class DynBufferV0, typename std::enable_if<
        !std::is_base_of<any_dynamic_buffer_v0_ref, DynBufferV0>::value
    >::type * = nullptr>
    explicit
    any_dynamic_buffer_v0_ref(DynBufferV0 &dynbuf);
    any_dynamic_buffer_v0_ref(any_dynamic_buffer_v0_ref const&) noexcept = default;
    any_dynamic_buffer_v0_ref& operator=(any_dynamic_buffer_v0_ref const&) noexcept = default;
    ~any_dynamic_buffer_v0_ref() = default;

    using const_buffers_type = any_const_buffers;
    using mutable_buffers_type = any_mutable_buffers;

    const_buffers_type
    data() const;

    mutable_buffers_type
    data();

    std::size_t
    max_size() const noexcept;

    std::size_t
    size() const noexcept;

    std::size_t
    capacity() const noexcept;

    void
    consume(std::size_t n);

    mutable_buffers_type
    prepare(std::size_t n);

    void
    commit(std::size_t n);

private:
    struct impl_concept
    {
        virtual
        const_buffers_type
        cdata(void*) const = 0;

        virtual
        mutable_buffers_type
        mdata(void*) const = 0;

        virtual
        std::size_t
        max_size(void*) const noexcept= 0;

        virtual
        std::size_t
        size(void*) const noexcept= 0;

        virtual
        std::size_t
        capacity(void*) const noexcept= 0;

        virtual
        mutable_buffers_type
        prepare(void* b, std::size_t n) const = 0;

        virtual
        void
        commit(void* b, std::size_t n) const = 0;

        virtual
        void
        consume(void* b, std::size_t n) const = 0;
    };

    template<class DynBufferV0>
    struct impl_model final
        : impl_concept
    {
        const_buffers_type
        cdata(void* p) const override
        {
            return const_buffers_type {
                static_cast<DynBufferV0 const*>(p)
                    ->data() };
        }

        mutable_buffers_type
        mdata(void* p) const override
        {
            return mutable_buffers_type {
                static_cast<DynBufferV0 *>(p)
                    ->data() };
        }

        std::size_t
        max_size(void* p) const noexcept override
        {
            return static_cast<DynBufferV0 const*>(p)
                ->max_size();
        }

        std::size_t
        size(void* p) const noexcept override
        {
            return static_cast<DynBufferV0 const*>(p)
                ->size();
        }

        std::size_t
        capacity(void* p) const noexcept override
        {
            return static_cast<DynBufferV0 const*>(p)
                ->capacity();
        }

        mutable_buffers_type
        prepare(void* b, std::size_t n) const override
        {
            return mutable_buffers_type {
                static_cast<DynBufferV0 *>(b)
                    ->prepare(n) };
        }

        void
        commit(void* p, std::size_t n) const override
        {
            return static_cast<DynBufferV0 *>(p)
                ->commit(n);
        }

        void
        consume(void* p, std::size_t n) const override
        {
            return static_cast<DynBufferV0 *>(p)
                ->consume(n);
        }

    };

    template<class DynBufferV0>
    auto build_model() ->
    impl_model<DynBufferV0> const*
    {
        static const impl_model<DynBufferV0> model {};
        return &model;
    }

    impl_concept const *impl_;
    void *dynbuf_;
};

template<class DynBufferV0, typename std::enable_if<
    !std::is_base_of<any_dynamic_buffer_v0_ref, DynBufferV0>::value
>::type * >
any_dynamic_buffer_v0_ref::any_dynamic_buffer_v0_ref(DynBufferV0 &dynbuf)
: impl_(build_model<DynBufferV0>())
, dynbuf_(static_cast<void*>(&dynbuf))
{
}

auto
any_dynamic_buffer_v0_ref::data() const
-> const_buffers_type
{
    return impl_->cdata(dynbuf_);
}

auto
any_dynamic_buffer_v0_ref::data()
-> mutable_buffers_type
{
    return impl_->mdata(dynbuf_);
}

auto
any_dynamic_buffer_v0_ref::max_size() const noexcept
-> std::size_t
{
    return impl_->max_size(dynbuf_);
}

auto
any_dynamic_buffer_v0_ref::size() const noexcept
-> std::size_t
{
    return impl_->size(dynbuf_);
}

auto
any_dynamic_buffer_v0_ref::capacity() const noexcept
-> std::size_t
{
    return impl_->capacity(dynbuf_);
}

auto
any_dynamic_buffer_v0_ref::prepare(std::size_t n)
-> mutable_buffers_type
{
    return impl_->prepare(dynbuf_, n);
}

void
any_dynamic_buffer_v0_ref::commit(std::size_t n)

{
    impl_->commit(dynbuf_, n);
}

void
any_dynamic_buffer_v0_ref::consume(std::size_t n)
{
    impl_->consume(dynbuf_, n);
}


}
}
}

#endif // BOOST_BEAST_CORE_DETAIL_ANY_DYNAMIC_BUFFER_V0_REF_HPP
