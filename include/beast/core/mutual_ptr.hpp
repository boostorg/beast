//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_MUTUAL_PTR_HPP
#define BEAST_MUTUAL_PTR_HPP

#include <cstdint>
#include <utility>

namespace beast {

namespace detail {
struct mutual_ptr_alloc {};
} // detail

/** A custom smart pointer.

    This smart pointer is used to manage an object allocated
    using an allocator. It maintains a non-thread-safe reference
    count. Instances of the smart pointer which track the same
    managed object are kept in a linked list. The @ref reset_all
    operation releases all pointers managing the same object.

    Objects of this type are used in the implementation of
    composed operations. Typically the composed operation's shared
    state is managed by the @ref mutual_ptr and an allocator
    associated with the final handler is used to create the managed
    object.
*/
template<class T>
class mutual_ptr
{
    struct B
    {
        T t;
        std::uint16_t n = 1;

        template<class... Args>
        explicit
        B(Args&&... args)
            : t(std::forward<Args>(args)...)
        {
        }

        virtual
        void
        destroy() = 0;
    };

    template<class Alloc>
    struct D;

    void
    release();

    B* p_;
    mutable mutual_ptr* next_;
    mutable mutual_ptr* prev_;

public:
    /** Destructor

        If an object is managed by this, the reference count
        will be decremented. If the reference count reaches
        zero, the managed object is destroyed.
    */
    ~mutual_ptr();

    /** Default constructor.

        Default constructed container have no managed object.
    */
    mutual_ptr();

    /** Move constructor.

        When this call returns, the moved-from container
        will have no managed object.
    */
    mutual_ptr(mutual_ptr&& other);

    /// Copy constructor
    mutual_ptr(mutual_ptr const& other);

    /** Move assignment

        When this call returns, the moved-from container
        will have no managed object.
    */
    mutual_ptr& operator=(mutual_ptr&& other);

    /// Copy assignment
    mutual_ptr& operator=(mutual_ptr const& other);

    /** Return a reference to the managed object. */
    T&
    operator*() const
    {
        return p_->t;
    }

    /** Return a pointer to the managed object. */
    T*
    operator->() const
    {
        return &(**this);
    }

    /** Returns the number of instances managing the current object.

        If there is no managed object, `0` is returned.
    */
    std::size_t
    use_count() const
    {
        return p_->n;
    }

    /** Release ownership of the managed object. */
    void
    reset();

    /** Reset all instances managing this object.

        This function releases all instances of the smart pointer
        which point to the same managed object, including this
        instance.

        Before the call, this must point to a managed object.
    */
    void
    reset_all();

public: // private
    template<class Alloc, class... Args>
    mutual_ptr(detail::mutual_ptr_alloc,
        Alloc const& alloc, Args&&... args);
};

template<class T, class Alloc, class... Args>
mutual_ptr<T>
allocate_mutual(Alloc const& a, Args&&... args)
{
    return mutual_ptr<T>{detail::mutual_ptr_alloc{},
        a, std::forward<Args>(args)...};
}

} // beast

#include <beast/core/impl/mutual_ptr.ipp>

#endif
