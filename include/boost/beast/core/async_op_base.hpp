//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_ASYNC_OP_BASE_HPP
#define BOOST_BEAST_CORE_ASYNC_OP_BASE_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/detail/allocator.hpp>
#include <boost/beast/core/detail/async_op_base.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/handler_alloc_hook.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/asio/post.hpp>
#include <boost/core/exchange.hpp>
#include <boost/core/empty_value.hpp>
#include <utility>

namespace boost {
namespace beast {

/** Base class to provide completion handler boilerplate for composed operations.

    A function object submitted to intermediate initiating functions during
    a composed operation may derive from this type to inherit all of the
    boilerplate to forward the executor, allocator, and legacy customization
    points associated with the completion handler invoked at the end of the
    composed operation.

    The composed operation must be typical; that is, associated with one
    executor of an I/O object, and invoking a caller-provided completion
    handler when the operation is finished. Classes derived from
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

    @par Example

    The following code demonstrates how @ref async_op_base may be be used to
    assist authoring an asynchronous initiating function, by providing all of
    the boilerplate to manage the final completion handler in a way that
    maintains the allocator and executor associations:

    @code

    // Asynchronously read into a buffer until the buffer is full, or an error occurs
    template<class AsyncReadStream, class ReadHandler>
    BOOST_ASIO_INITFN_RESULT_TYPE(ReadHandler, void (error_code, std::size_t))
    async_read(AsyncReadStream& stream, net::mutable_buffer buffer, ReadHandler&& handler)
    {
        using handler_type = BOOST_ASIO_HANDLER_TYPE(ReadHandler, void(error_code, std::size_t));
        using base_type = async_op_base<handler_type, typename AsyncReadStream::executor_type>;

        struct op : base_type
        {
            AsyncReadStream& stream_;
            net::mutable_buffer buffer_;
            std::size_t total_bytes_transferred_;

            op(
                AsyncReadStream& stream,
                net::mutable_buffer buffer,
                handler_type& handler)
                : base_type(std::move(handler), stream.get_executor())
                , stream_(stream)
                , buffer_(buffer)
                , total_bytes_transferred_(0)
            {
                (*this)({}, 0, false); // start the operation
            }

            void operator()(error_code ec, std::size_t bytes_transferred, bool is_continuation = true)
            {
                // Adjust the count of bytes and advance our buffer
                total_bytes_transferred_ += bytes_transferred;
                buffer_ = buffer_ + bytes_transferred;

                // Keep reading until buffer is full or an error occurs
                if(! ec && buffer_.size() > 0)
                    return stream_.async_read_some(buffer_, std::move(*this));

                // Call the completion handler with the result. If `is_continuation` is
                // false, which happens on the first time through this function, then
                // `net::post` will be used to call the completion handler, otherwise
                // the completion handler will be invoked directly.

                this->invoke(is_continuation, ec, total_bytes_transferred_);
            }
        };

        net::async_completion<ReadHandler, void(error_code, std::size_t)> init{handler};
        op(stream, buffer, init.completion_handler);
        return init.result.get();
    }

    @endcode

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

    @see @ref stable_async_op_base
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
        "Executor type requirements not met");

    Handler h_;
    net::executor_work_guard<Executor1> wg1_;

    virtual
    void
    before_invoke_hook()
    {
    }

public:
    /** Constructor

        @param handler The final completion handler.
        The type of this object must meet the requirements of <em>CompletionHandler</em>.
        The implementation takes ownership of the handler by performing a decay-copy.

        @param ex1 The executor associated with the implied I/O object
        target of the operation. The implementation shall maintain an
        executor work guard for the lifetime of the operation, or until
        the final completion handler is invoked, whichever is shorter.

        @param alloc The allocator to be associated with objects
        derived from this class. If `Allocator` is default-constructible,
        this parameter is optional and may be omitted.
    */
#if BOOST_BEAST_DOXYGEN
    template<class Handler_>
    async_op_base(
        Handler&& handler,
        Executor1 const& ex1,
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
        Handler_&& handler,
        Executor1 const& ex1)
        : h_(std::forward<Handler_>(handler))
        , wg1_(ex1)
    {
    }

    template<class Handler_>
    async_op_base(
        Handler_&& handler,
        Executor1 const& ex1,
        Allocator const& alloc)
        : boost::empty_value<Allocator>(
            boost::empty_init_t{}, alloc)
        , h_(std::forward<Handler_>(handler))
        , wg1_(ex1)
    {
    }
#endif

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
            h_, wg1_.get_executor());
    }

    /// Returns the handler associated with this object
    Handler const&
    handler() const noexcept
    {
        return h_;
    }

    /** Returns ownership of the handler associated with this object

        This function is used to transfer ownership of the handler to
        the caller, by move-construction. After the move, the only
        valid operations on the base object are move construction and
        destruction.
    */
    Handler
    release_handler()
    {
        return std::move(h_);
    }

    /** Invoke the final completion handler, maybe using post.

        This invokes the final completion handler with the specified
        arguments forwarded. It is undefined to call either of
        @ref invoke or @ref invoke_now more than once.

        Any temporary objects allocated with @ref allocate_stable will
        be automatically destroyed before the final completion handler
        is invoked.

        @param is_continuation If this value is `false`, then the
        handler will be submitted to the executor using `net::post`.
        Otherwise the handler will be invoked as if by calling
        @ref invoke_now.

        @param args A list of optional parameters to invoke the handler
        with. The completion handler must be invocable with the parameter
        list, or else a compilation error will result.
    */
    template<class... Args>
    void
    invoke(bool is_continuation, Args&&... args)
    {
        this->before_invoke_hook();
        if(! is_continuation)
        {
            net::post(net::bind_executor(
                wg1_.get_executor(),
                beast::bind_front_handler(
                    std::move(h_),
                    std::forward<Args>(args)...)));
            wg1_.reset();
        }
        else
        {
            h_(std::forward<Args>(args)...);
            wg1_.reset();
        }
    }

    /** Invoke the final completion handler.

        This invokes the final completion handler with the specified
        arguments forwarded. It is undefined to call either of
        @ref invoke or @ref invoke_now more than once.

        Any temporary objects allocated with @ref allocate_stable will
        be automatically destroyed before the final completion handler
        is invoked.

        @param args A list of optional parameters to invoke the handler
        with. The completion handler must be invocable with the parameter
        list, or else a compilation error will result.
    */
    template<class... Args>
    void
    invoke_now(Args&&... args)
    {
        this->before_invoke_hook();
        wg1_.reset();
        h_(std::forward<Args>(args)...);
    }

#if ! BOOST_BEAST_DOXYGEN
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

//------------------------------------------------------------------------------

/** Base class to provide completion handler boilerplate for composed operations.

    A function object submitted to intermediate initiating functions during
    a composed operation may derive from this type to inherit all of the
    boilerplate to forward the executor, allocator, and legacy customization
    points associated with the completion handler invoked at the end of the
    composed operation.

    The composed operation must be typical; that is, associated with one
    executor of an I/O object, and invoking a caller-provided completion
    handler when the operation is finished. Classes derived from
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

    @par Example

    The following code demonstrates how @ref stable_async_op_base may be be used to
    assist authoring an asynchronous initiating function, by providing all of
    the boilerplate to manage the final completion handler in a way that maintains
    the allocator and executor associations. Furthermore, the operation shown
    allocates temporary memory using @ref allocate_stable for the timer and
    message, whose addresses must not change between intermediate operations:

    @code

    // Asynchronously send a message multiple times, once per second
    template <class AsyncWriteStream, class T, class WriteHandler>
    auto async_write_messages(
        AsyncWriteStream& stream,
        T const& message,
        std::size_t repeat_count,
        WriteHandler&& handler) ->
            typename net::async_result<
                typename std::decay<WriteHandler>::type,
                void(error_code)>::return_type
    {
        using handler_type = typename net::async_completion<WriteHandler, void(error_code)>::completion_handler_type;
        using base_type = stable_async_op_base<handler_type, typename AsyncWriteStream::executor_type>;

        struct op : base_type
        {
            // This object must have a stable address
            struct temporary_data
            {
                std::string const message;
                net::steady_timer timer;

                temporary_data(std::string message_, net::io_context& ctx)
                    : message(std::move(message_))
                    , timer(ctx)
                {
                }
            };

            AsyncWriteStream& stream_;
            std::size_t repeats_;
            temporary_data& data_;
            enum { starting, waiting, writing } state_;

            op(AsyncWriteStream& stream, std::size_t repeats, std::string message, handler_type& handler)
                : base_type(std::move(handler), stream.get_executor())
                , stream_(stream)
                , repeats_(repeats)
                , state_(starting)
                , data_(allocate_stable<temporary_data>(*this, std::move(message), stream.get_executor().context()))
            {
                (*this)(); // start the operation
            }

            void operator()(error_code ec = {}, std::size_t = 0)
            {
                if (!ec)
                {
                    switch (state_)
                    {
                    case starting:
                        // If repeats starts at 0 then we must complete immediately. But we can't call the final
                        // handler from inside the initiating function, so we post our intermediate handler first.
                        if(repeats_ == 0)
                            return net::post(std::move(*this));

                    case writing:
                        if (repeats_ > 0)
                        {
                            --repeats_;
                            state_ = waiting;
                            data_.timer.expires_after(std::chrono::seconds(1));

                            // Composed operation not yet complete.
                            return data_.timer.async_wait(std::move(*this));
                        }

                        // Composed operation complete, continue below.
                        break;

                    case waiting:
                        // Composed operation not yet complete.
                        state_ = writing;
                        return net::async_write(stream_, net::buffer(data_.message), std::move(*this));
                    }
                }

                // The base class destroys the temporary data automatically, before invoking the final completion handler
                this->invoke_now(ec);
            }
        };

        net::async_completion<WriteHandler, void(error_code)> completion(handler);
        std::ostringstream os;
        os << message;
        op(stream, repeat_count, os.str(), completion.completion_handler);
        return completion.result.get();
    }

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

    @see @ref allocate_stable, @ref async_op_base
*/
template<
    class Handler,
    class Executor1,
    class Allocator = std::allocator<void>
>
class stable_async_op_base
    : public async_op_base<
        Handler, Executor1, Allocator>
{
    detail::stable_base* list_ = nullptr;

    void
    before_invoke_hook() override
    {
        detail::stable_base::destroy_list(list_);
    }

public:
    /** Constructor

        @param handler The final completion handler.
        The type of this object must meet the requirements of <em>CompletionHandler</em>.
        The implementation takes ownership of the handler by performing a decay-copy.

        @param ex1 The executor associated with the implied I/O object
        target of the operation. The implementation shall maintain an
        executor work guard for the lifetime of the operation, or until
        the final completion handler is invoked, whichever is shorter.

        @param alloc The allocator to be associated with objects
        derived from this class. If `Allocator` is default-constructible,
        this parameter is optional and may be omitted.
    */
#if BOOST_BEAST_DOXYGEN
    template<class Handler>
    stable_async_op_base(
        Handler&& handler,
        Executor1 const& ex1,
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
        Handler_&& handler,
        Executor1 const& ex1)
        : async_op_base<
            Handler, Executor1, Allocator>(
                std::forward<Handler_>(handler), ex1)
    {
    }

    template<class Handler_>
    stable_async_op_base(
        Handler_&& handler,
        Executor1 const& ex1,
        Allocator const& alloc)
        : async_op_base<
            Handler, Executor1, Allocator>(
                std::forward<Handler_>(handler), ex1, alloc)
    {
    }
#endif

    /// Move Constructor
    stable_async_op_base(stable_async_op_base&& other)
        : async_op_base<Handler, Executor1, Allocator>(
            std::move(other))
        , list_(boost::exchange(other.list_, nullptr))
    {
    }

    /** Destructor

        If the completion handler was not invoked, then any
        state objects allocated with @ref allocate_stable will
        be destroyed here.
    */
    ~stable_async_op_base()
    {
        detail::stable_base::destroy_list(list_);
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

namespace detail {

template<class State, class Allocator>
struct allocate_stable_state final
    : stable_base
    , boost::empty_value<Allocator>
{
    State value;

    template<class... Args>
    explicit
    allocate_stable_state(
        Allocator const& alloc,
        Args&&... args)
        : boost::empty_value<Allocator>(
            boost::empty_init_t{}, alloc)
        , value{std::forward<Args>(args)...}
    {
    }

    void destroy() override
    {
        using A = typename allocator_traits<
            Allocator>::template rebind_alloc<
                allocate_stable_state>;

        A a(this->get());
        detail::allocator_traits<A>::destroy(a, this);
        detail::allocator_traits<A>::deallocate(a, this, 1);
    }
};

} // detail

/** Allocate a temporary object to hold stable asynchronous operation state.

    The object will be destroyed just before the completion
    handler is invoked, or when the base is destroyed.

    @see @ref stable_async_op_base
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
    using allocator_type = typename stable_async_op_base<
        Handler, Executor1, Allocator>::allocator_type;

    using A = typename detail::allocator_traits<
        allocator_type>::template rebind_alloc<
            detail::allocate_stable_state<
                State, allocator_type>>;

    struct deleter
    {
        allocator_type alloc;
        detail::allocate_stable_state<
            State, allocator_type>* ptr;

        ~deleter()
        {
            if(ptr)
            {
                A a(alloc);
                detail::allocator_traits<A>::deallocate(a, ptr, 1);
            }
        }
    };

    A a(base.get_allocator());
    deleter d{base.get_allocator(), nullptr};
    d.ptr = detail::allocator_traits<A>::allocate(a, 1);
    detail::allocator_traits<A>::construct(a, d.ptr,
        d.alloc, std::forward<Args>(args)...);
    d.ptr->next_ = base.list_;
    base.list_ = d.ptr;
    return boost::exchange(d.ptr, nullptr)->value;
}

} // beast
} // boost

#endif
