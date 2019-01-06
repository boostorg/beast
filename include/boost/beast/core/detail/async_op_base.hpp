//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DETAIL_ASYNC_OP_BASE_HPP
#define BOOST_BEAST_CORE_DETAIL_ASYNC_OP_BASE_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/handler_alloc_hook.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/core/exchange.hpp>
#include <boost/core/empty_value.hpp>
#include <utility>

namespace boost {
namespace beast {
namespace detail {

//------------------------------------------------------------------------------

/** Base class to provide completion handler boilerplate for composed operations.

    A function object submitted to intermediate initiating functions during
    a composed operation may derive from this type to inherit all of the
    boilerplate to forward the executor, allocator, and legacy customization
    points associated with the completion handler invoked at the end of the
    composed operation.

    The composed operation must be typical; that is, associated with the executor
    of a particular I/O object, and invoking a caller-provided completion handler
    when the operation is finished. Specifically, classes derived from
    @ref async_op_base will acquire these properties:

    @li Ownership of the final completion handler provided upon construction.

    @li If the final handler has an associated allocator, this allocator will
        be propagated to the composed operation subclass. Otherwise, the
        associated allocator will be the type specified in the allocator
        template parameter, or the default of `std::allocator<void>` if the
        parameter is omitted.

    @li If the final handler has an associated executor, then it will be used
        as the executor associated with the composed operation. Otherwise,
        the specified `Executor1` will be the type of executor associated
        with the composed operation.

    @li An instance of `net::executor_work_guard` for the instance of `Executor1`
        shall be maintained until either the final handler is invoked, or the
        operation base is destroyed, whichever comes first.

    @li Calls to the legacy customization points
        `asio_handler_invoke`,
        `asio_handler_allocate`,
        `asio_handler_deallocate`, and
        `asio_handler_is_continuation`,
        which use argument-dependent lookup, will be forwarded to the
        legacy customization points associated with the handler.

    Data members of composed operations implemented as completion handlers
    do not have stable addresses, as the composed operation object is move
    constructed upon each call to an initiating function. For most operations
    this is not a problem. For complex operations requiring stable temporary
    storage, the class @ref stable_async_op_base is provided which offers
    additional functionality:

    @li The free function @ref allocate_stable may be used to allocate
    one or more temporary objects associated with the composed operation.

    @li Memory for stable temporary objects is allocated using the allocator
    associated with the composed operation.

    @li Stable temporary objects are automatically destroyed, and the memory
    freed using the associated allocator, either before the final completion
    handler is invoked (a Networking requirement) or when the composed operation
    is destroyed, whichever occurs first.

    @par Temporary Storage Example

    The following example demonstrates how a composed operation may store a
    temporary object.

    @code

    @endcode

    @tparam Handler The type of the completion handler to store.
    This type must meet the requirements of <em>CompletionHandler</em>.

    @tparam Executor1 The type of the executor used when the handler has no
    associated executor. An instance of this type must be provided upon
    construction. The implementation will maintain an executor work guard
    and a copy of this instance.

    @tparam Allocator The allocator type to use if the handler does not
    have an associated allocator. If this parameter is omitted, then
    `std::allocator<void>` will be used. If the specified allocator is
    not default constructible, an instance of the type must be provided
    upon construction.

    @see @ref allocate_stable
*/
template<
    class Handler,
    class Executor1,
    class Allocator = std::allocator<void>
>
class async_op_base
#if ! BOOST_BEAST_DOXYGEN
    : private boost::empty_value<Allocator>
#endif
{
    static_assert(
        net::is_executor<Executor1>::value,
        "Executor requirements not met");

    Handler h_;
    net::executor_work_guard<Executor1> wg_;

    virtual
    void
    before_invoke_hook()
    {
    }

public:
    /// Move Constructor
    async_op_base(async_op_base&& other) = default;

    /** The type of allocator associated with this object.

        If a class derived from @ref async_op_base is a completion
        handler, then the associated allocator of the derived class will
        be this type.
    */
    using allocator_type =
        net::associated_allocator_t<Handler, Allocator>;

    /** The type of executor associated with this object.

        If a class derived from @ref async_op_base is a completion
        handler, then the associated executor of the derived class will
        be this type.
    */
    using executor_type =
        net::associated_executor_t<Handler, Executor1>;

    /** Returns the allocator associated with this object.

        If a class derived from @ref async_op_base is a completion
        handler, then the object returned from this function will be used
        as the associated allocator of the derived class.
    */
    allocator_type
    get_allocator() const noexcept
    {
        return net::get_associated_allocator(h_,
            boost::empty_value<Allocator>::get());
    }

    /** Returns the executor associated with this object.

        If a class derived from @ref async_op_base is a completion
        handler, then the object returned from this function will be used
        as the associated executor of the derived class.
    */
    executor_type
    get_executor() const noexcept
    {
        return net::get_associated_executor(
            h_, wg_.get_executor());
    }

    /// Returns the handler associated with this object
    Handler const&
    handler() const noexcept
    {
        return h_;
    }

protected:
    /** Constructor

        @param target The target of the operation. This object must
        have class type, with a member function named `get_executor`
        publicly accessible whose return type meets the requirements
        of <em>Executor</em>.

        @param handler The final completion handler. The type of this
        object must meet the requirements of <em>CompletionHandler</em>.

        @param alloc The allocator to be associated with objects
        derived from this class. If `Allocator` is default-constructible,
        this parameter is optional and may be omitted.
    */
#if BOOST_BEAST_DOXYGEN
    template<class Handler>
    async_op_base(
        Executor1 const& ex1,
        Handler&& handler,
        Allocator const& alloc = Allocator());
#else
    template<
        class Handler_,
        class = typename std::enable_if<
            ! std::is_same<typename
                std::decay<Handler_>::type,
                async_op_base
            >::value>::type
    >
    async_op_base(
        Executor1 const& ex1,
        Handler_&& handler)
        : h_(std::forward<Handler_>(handler))
        , wg_(ex1)
    {
    }

    template<class Handler_>
    async_op_base(
        Executor1 const& ex1,
        Handler_&& handler,
        Allocator const& alloc)
        : boost::empty_value<Allocator>(alloc)
        , h_(std::forward<Handler_>(handler))
        , wg_(ex1)
    {
    }
#endif

    /** Invoke the final completion handler.

        This invokes the final completion handler with the specified
        arguments forwarded. It is undefined to call this member
        function more than once.
    */
    template<class... Args>
    void
    invoke(Args&&... args)
    {
        this->before_invoke_hook();
        wg_.reset();
        h_(std::forward<Args>(args)...);
    }

#if ! BOOST_BEAST_DOXYGEN
public:
    template<
        class Handler_,
        class Executor1_,
        class Allocator_,
        class Function>
    friend
    void asio_handler_invoke(
        Function&& f,
        async_op_base<
            Handler_, Executor1_, Allocator_>* p);

    friend
    void* asio_handler_allocate(
        std::size_t size, async_op_base* p)
    {
        using net::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(p->h_));
    }

    friend
    void asio_handler_deallocate(
        void* mem, std::size_t size,
            async_op_base* p)
    {
        using net::asio_handler_deallocate;
        asio_handler_deallocate(mem, size,
            std::addressof(p->h_));
    }

    friend
    bool asio_handler_is_continuation(
        async_op_base* p)
    {
        using net::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(p->h_));
    }
#endif
};

#if ! BOOST_BEAST_DOXYGEN
template<
    class Handler,
    class Executor1,
    class Allocator,
    class Function>
void asio_handler_invoke(
    Function&& f,
    async_op_base<
        Handler, Executor1, Allocator>* p)
{
    using net::asio_handler_invoke;
    asio_handler_invoke(
        f, std::addressof(p->h_));
}
#endif

//------------------------------------------------------------------------------

// namspace detail {

class stable_async_op_detail
{
protected:
    struct stable_base
    {
        static
        void
        destroy_list(stable_base*& list)
        {
            while(list)
            {
                auto next = list->next_;
                list->destroy();
                list = next;
            }
        }

    protected:
        stable_base* next_;
        virtual void destroy() = 0;
        virtual ~stable_base() = default;
        explicit stable_base(stable_base*& list)
            : next_(boost::exchange(list, this))
        {
        }
    };
};

// } detail

template<
    class Handler,
    class Executor1,
    class Allocator = std::allocator<void>
>
class stable_async_op_base
    : public async_op_base<
        Handler, Executor1, Allocator>
#if ! BOOST_BEAST_DOXYGEN
    , private stable_async_op_detail
#endif
{
    stable_base* list_ = nullptr;

    void
    before_invoke_hook() override
    {
        stable_base::destroy_list(list_);
    }

public:
    /// Move Constructor
    stable_async_op_base(stable_async_op_base&& other)
        : async_op_base<Handler, Executor1, Allocator>(
            std::move(other))
        , list_(boost::exchange(other.list_, nullptr))
    {
    }

protected:
    /** Constructor

        @param target The target of the operation. This object must
        have class type, with a member function named `get_executor`
        publicly accessible whose return type meets the requirements
        of <em>Executor</em>.

        @param handler The final completion handler. The type of this
        object must meet the requirements of <em>CompletionHandler</em>.

        @param alloc The allocator to be associated with objects
        derived from this class. If `Allocator` is default-constructible,
        this parameter is optional and may be omitted.
    */
#if BOOST_BEAST_DOXYGEN
    template<class Handler>
    stable_async_op_base(
        Executor1 const& ex1,
        Handler&& handler,
        Allocator const& alloc = Allocator());
#else
    template<
        class Handler_,
        class = typename std::enable_if<
            ! std::is_same<typename
                std::decay<Handler_>::type,
                stable_async_op_base
            >::value>::type
    >
    stable_async_op_base(
        Executor1 const& ex1,
        Handler_&& handler)
        : async_op_base<
            Handler, Executor1, Allocator>(
                ex1, std::forward<Handler_>(handler))
    {
    }

    template<class Handler_>
    stable_async_op_base(
        Executor1 const& ex1,
        Handler_&& handler,
        Allocator const& alloc)
        : async_op_base<
            Handler, Executor1, Allocator>(
                ex1, std::forward<Handler_>(handler))
    {
    }
#endif

    /** Destructor

        If the completion handler was not invoked, then any
        state objects allocated with @ref allocate_stable will
        be destroyed here.
    */
    ~stable_async_op_base()
    {
        stable_base::destroy_list(list_);
    }

    /** Allocate a temporary object to hold operation state.

        The object will be destroyed just before the completion
        handler is invoked, or when the operation base is destroyed.
    */
    template<
        class State,
        class Handler_,
        class Executor1_,
        class Allocator_,
        class... Args>
    friend
    State&
    allocate_stable(
        stable_async_op_base<
            Handler_, Executor1_, Allocator_>& base,
        Args&&... args);
};

/** Allocate a temporary object to hold stable asynchronous operation state.

    The object will be destroyed just before the completion
    handler is invoked, or when the base is destroyed.
*/
template<
    class State,
    class Handler,
    class Executor1,
    class Allocator,
    class... Args>
State&
allocate_stable(
    stable_async_op_base<
        Handler, Executor1, Allocator>& base,
    Args&&... args)
{
    using base_type = stable_async_op_base<
        Handler, Executor1, Allocator>;

    using stable_base =
        typename base_type::stable_base;

    struct state final
        : stable_base
    {
        State value;

        explicit
        state(
            stable_base*& list,
            Args&&... args)
            : stable_base(list)
            , value(std::forward<Args>(args)...)
        {
        }

        void destroy() override
        {
            delete this;
        }
    };

    return (::new state(base.list_,
        std::forward<Args>(args)...))->value;
}

} // detail
} // beast
} // boost

#endif
