//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DETAIL_OPERATION_BASE_HPP
#define BOOST_BEAST_CORE_DETAIL_OPERATION_BASE_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/handler_alloc_hook.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/asio/system_executor.hpp>
#include <boost/core/empty_value.hpp>
#include <utility>

namespace boost {
namespace beast {
namespace detail {

/** Base class which stores a handler and forwards handler associations.

    This mix-in assists bind wrappers, intermediate handlers, composed
    operations, and similar types which store a handler, by acting as
    a base class which holds the handler. Any networking customizations
    on the handler will be propagated to the derived class. Specifically:

    @li Any allocator associated with the handler will be propagated to
        this object. Otherwise, the allocator associated with this
        object will be a default allocator which the caller may specify
        through a template parameter and constructor parameter.

    @li Any executor associated with the handler will be propagated to
        this object. Otherwise, the executor associated with this
        object will be a default executor which the caller may specify
        through a template parameter and constructor parameter.

    @li The legacy customization points
        `asio_handler_invoke`,
        `asio_handler_allocate`,
        `asio_handler_deallocate`, and
        `asio_handler_is_continuation`,
        which use argument-dependent lookup, will be forwarded to the
        legacy customization points associated with the handler.

    @par Example

    The following declaration produces a class which wraps a
    handler and inherits all of the networking customizations
    associated with that handler:

    @code
    template <class Handler>
    struct wrapped_handler : operation_base<
        Handler, net::associated_executor_t<Handler>>
    {
        template <class Handler_>
        explicit wrapped_handler (Handler_&& handler)
            : operation_base<Handler, net::associated_executor_t <Handler>>(
                std::forward<Handler_>(handler), net::get_associated_executor(handler))
        {
        }

        template <class... Args>
        void operator()(Args&&... args)
        {
            this->handler_(std::forward<Args>(args)...);
        }
    };
    @endcode

    @tparam Handler The type of the completion handler to store.
    This type must meet the requirements of <em>CompletionHandler</em>.

    @tparam Executor The executor type to use if the handler does not
    have an associated executor.

    @tparam Allocator The allocator type to use if the handler does not
    have an associated allocator. If this parameter is omitted, then
    `std::allocator<void>` will be used.
*/
template<
    class Handler,
    class Executor,
    class Allocator = std::allocator<void>
>
class operation_base
#if ! BOOST_BEAST_DOXYGEN
    : private boost::empty_value<
        net::associated_allocator_t<
            Handler, Allocator>, 0>
    , private boost::empty_value<
        net::associated_executor_t<
            Handler, Executor>, 1>
#endif
{
public:
    /** The type of allocator associated with this object.

        If a class derived from @ref operation_base is a completion
        handler, then the associated allocator of the derived class will
        be this type.
    */
    using allocator_type =
        net::associated_allocator_t<
            Handler, Allocator>;

    /** The type of executor associated with this object.

        If a class derived from @ref operation_base is a completion
        handler, then the associated executor of the derived class will
        be this type.
    */
    using executor_type =
        net::associated_executor_t<
            Handler, Executor>;

    /** Returns the allocator associated with this object.

        If a class derived from @ref operation_base is a completion
        handler, then the object returned from this function will be used
        as the associated allocator of the derived class.
    */
    allocator_type
    get_allocator() const noexcept
    {
        return boost::empty_value<
            allocator_type, 0>::get();
    }

    /** Returns the allocator associated with this object.

        If a class derived from @ref operation_base is a completion
        handler, then the object returned from this function will be used
        as the associated allocator of the derived class.
    */
    executor_type
    get_executor() const noexcept
    {
        return boost::empty_value<
            executor_type, 1>::get();
    }

protected:
    Handler handler_;

    template<
        class DeducedHandler
#if BOOST_BEAST_DOXYGEN
        ,class = typename std::enable_if<
            ! std::is_same<typename
                std::decay<Handler_>::type,
                operation_base
            >::value>::type
#endif
    >
    operation_base(
        DeducedHandler&& handler,
        executor_type ex = executor_type{},
        allocator_type alloc = allocator_type{})
        : boost::empty_value<allocator_type, 0>(
            boost::empty_init_t{}, alloc)
        , boost::empty_value<executor_type, 1>(
            boost::empty_init_t{}, ex)
        , handler_(std::forward<DeducedHandler>(handler))
    {
    }

public:
#if ! BOOST_BEAST_DOXYGEN
    template<
        class Handler_,
        class Executor_,
        class Allocator_,
        class Function>
    friend
    void asio_handler_invoke(
        Function&& f,
        operation_base<
            Handler_, Executor_, Allocator_>* p);

    friend
    void* asio_handler_allocate(
        std::size_t size, operation_base* p)
    {
        using net::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(p->handler_));
    }

    friend
    void asio_handler_deallocate(
        void* mem, std::size_t size,
            operation_base* p)
    {
        using net::asio_handler_deallocate;
        asio_handler_deallocate(mem, size,
            std::addressof(p->handler_));
    }

    friend
    bool asio_handler_is_continuation(
        operation_base* p)
    {
        using net::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(p->handler_));
    }
#endif
};

#if ! BOOST_BEAST_DOXYGEN

template<
    class Handler,
    class Executor,
    class Allocator,
    class Function>
void asio_handler_invoke(
    Function&& f,
    operation_base<
        Handler, Executor, Allocator>* p)
{
    using net::asio_handler_invoke;
    asio_handler_invoke(
        f, std::addressof(p->handler_));
}

#endif

} // detail
} // beast
} // boost

#endif
