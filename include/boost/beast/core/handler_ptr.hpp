//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HANDLER_PTR_HPP
#define BOOST_BEAST_HANDLER_PTR_HPP

#include <boost/beast/core/detail/allocator.hpp>
#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/type_traits.hpp>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast {

/** A smart pointer container with associated completion handler.

    This is a smart pointer that retains unique ownership of an
    object through a pointer. Memory is managed using the allocation
    and deallocation functions associated with a completion handler,
    which is also stored in the object. The managed object is
    destroyed and its memory deallocated when one of the following
    happens:

    @li The function @ref invoke is called.

    @li The function @ref release_handler is called.

    @li The owning the object is destroyed.

    Objects of this type are used in the implementation of
    composed operations. Typically the composed operation's
    state is managed by the @ref handler_ptr and an allocator
    associated with the final handler is used to create the managed
    object.

    @par Thread Safety
    @e Distinct @e objects: Safe.@n
    @e Shared @e objects: Unsafe.

    @tparam T The type of the owned object. Must be noexcept destructible.

    @tparam Handler The type of the completion handler.
*/
template<class T, class Handler>
class handler_ptr
{
    T* t_ = nullptr;
    Handler h_;

    void clear();
public:
    static_assert(std::is_nothrow_destructible<T>::value,
        "T must be nothrow destructible");

    /// The type of element this object stores
    using element_type = T;

    /// The type of handler this object stores
    using handler_type = Handler;

    /// Copy assignment (deleted).
    handler_ptr& operator=(handler_ptr const&) = delete;

    /// Move assignment (deleted).
    handler_ptr& operator=(handler_ptr &&) = delete;

    /** Destructs the owned object if no more @ref handler_ptr link to it.

        If `*this` owns an object the object is destroyed and
        the memory deallocated using the associated deallocator.
    */
    ~handler_ptr();

    /// Default constructor (deleted).
    handler_ptr() = delete;

    /** Move constructor.

        When this call returns, the moved-from container
        will have no owned object.
    */
    handler_ptr(handler_ptr&& other);

    /// Copy constructor (deleted).
    handler_ptr(handler_ptr const& other) = delete;


    /** Construct a new @ref handler_ptr

        This creates a new @ref handler_ptr with an owned object
        of type `T`. The allocator associated with the handler will
        be used to allocate memory for the owned object. The constructor
        for the owned object will be called thusly:

        @code
            T(handler, std::forward<Args>(args)...)
        @endcode

        @param handler The handler to associate with the owned
        object. The argument will be moved if it is an xvalue (if allocation
        or construction of T throws an exception, the argument is left
        in a moved-from state).

        @param args Optional arguments forwarded to
        the owned object's constructor.
    */
    template<class DeducedHandler, class... Args>
    explicit handler_ptr(DeducedHandler&& handler, Args&&... args);

    /// Returns a const reference to the handler
    handler_type const&
    handler() const
    {
        return h_;
    }

    /// Returns a reference to the handler
    handler_type&
    handler()
    {
        return h_;
    }

    /** Returns a pointer to the owned object.
    */
    T*
    get() const
    {
        return t_;
    }

    /// Return a reference to the owned object.
    T&
    operator*() const
    {
        return *t_;
    }

    /// Return a pointer to the owned object.
    T*
    operator->() const
    {
        return t_;
    }

    /** Release ownership of the handler

        Requires: `*this` owns an object

        Before this function returns,
        the owned object is destroyed, satisfying the
        deallocation-before-invocation Asio guarantee.
        @return The released handler.
    */
    handler_type
    release_handler();

    /** Invoke the handler in the owned object.

        This function invokes the handler in the owned object
        with a forwarded argument list. Before the invocation,
        the owned object is destroyed, satisfying the
        deallocation-before-invocation Asio guarantee.

        @note Care must be taken when the arguments are themselves
        stored in the owned object. Such arguments must first be
        moved to the stack or elsewhere, and then passed, or else
        undefined behavior will result.
    */
    template<class... Args>
    void
    invoke(Args&&... args);
};

} // beast
} // boost

#include <boost/beast/core/impl/handler_ptr.ipp>

#endif
