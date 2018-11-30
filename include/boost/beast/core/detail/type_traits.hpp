//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_DETAIL_TYPE_TRAITS_HPP
#define BOOST_BEAST_DETAIL_TYPE_TRAITS_HPP

#include <boost/beast/core/error.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/mp11/function.hpp>
#include <boost/type_traits.hpp>
#include <iterator>
#include <tuple>
#include <type_traits>
#include <string>
#include <utility>

namespace boost {
namespace beast {
namespace detail {

template<class U>
inline
std::size_t constexpr
max_sizeof()
{
    return sizeof(U);
}

template<class U0, class U1, class... Us>
inline
std::size_t constexpr
max_sizeof()
{
    return
        max_sizeof<U0>() > max_sizeof<U1, Us...>() ?
        max_sizeof<U0>() : max_sizeof<U1, Us...>();
}

template<class U>
inline
std::size_t constexpr
max_alignof()
{
    return alignof(U);
}

template<class U0, class U1, class... Us>
std::size_t constexpr
max_alignof()
{
    return
        max_alignof<U0>() > max_alignof<U1, Us...>() ?
        max_alignof<U0>() : max_alignof<U1, Us...>();
}

// (since C++17)
template<class... Ts>
using make_void = boost::make_void<Ts...>;
template<class... Ts>
using void_t = boost::void_t<Ts...>;

// (since C++11) missing from g++4.8
template<std::size_t Len, class... Ts>
struct aligned_union
{
    static
    std::size_t constexpr alignment_value =
        max_alignof<Ts...>();

    using type = typename std::aligned_storage<
        (Len > max_sizeof<Ts...>()) ? Len : (max_sizeof<Ts...>()),
            alignment_value>::type;
};

template<std::size_t Len, class... Ts>
using aligned_union_t =
    typename aligned_union<Len, Ts...>::type;

//------------------------------------------------------------------------------

template<class T>
inline
void
accept_rv(T){}

//------------------------------------------------------------------------------

template<class R, class C, class ...A>
auto
is_invocable_test(C&& c, int, A&& ...a)
    -> decltype(std::is_convertible<
        decltype(c(std::forward<A>(a)...)), R>::value ||
            std::is_same<R, void>::value,
                std::true_type());

template<class R, class C, class ...A>
std::false_type
is_invocable_test(C&& c, long, A&& ...a);

/** Metafunction returns `true` if F callable as R(A...)

    Example:

    @code
        is_invocable<T, void(std::string)>
    @endcode
*/
/** @{ */
template<class C, class F>
struct is_invocable : std::false_type
{
};

template<class C, class R, class ...A>
struct is_invocable<C, R(A...)>
    : decltype(is_invocable_test<R>(
        std::declval<C>(), 1, std::declval<A>()...))
{
};
/** @} */

//------------------------------------------------------------------------------

// for span
template<class T, class E, class = void>
struct is_contiguous_container: std::false_type {};

template<class T, class E>
struct is_contiguous_container<T, E, void_t<
    decltype(
        std::declval<std::size_t&>() = std::declval<T const&>().size(),
        std::declval<E*&>() = std::declval<T&>().data()),
    typename std::enable_if<
        std::is_same<
            typename std::remove_cv<E>::type,
            typename std::remove_cv<
                typename std::remove_pointer<
                    decltype(std::declval<T&>().data())
                >::type
            >::type
        >::value
    >::type>>: std::true_type
{};

//------------------------------------------------------------------------------

template<class...>
struct unwidest_unsigned;

template<class U0>
struct unwidest_unsigned<U0>
{
    using type = U0;
};

template<class U0, class... UN>
struct unwidest_unsigned<U0, UN...>
{
    BOOST_STATIC_ASSERT(std::is_unsigned<U0>::value);
    using type = typename std::conditional<
        (sizeof(U0) < sizeof(typename unwidest_unsigned<UN...>::type)),
        U0, typename unwidest_unsigned<UN...>::type>::type;
};

template<class...>
struct widest_unsigned;

template<class U0>
struct widest_unsigned<U0>
{
    using type = U0;
};

template<class U0, class... UN>
struct widest_unsigned<U0, UN...>
{
    BOOST_STATIC_ASSERT(std::is_unsigned<U0>::value);
    using type = typename std::conditional<
        (sizeof(U0) > sizeof(typename widest_unsigned<UN...>::type)),
        U0, typename widest_unsigned<UN...>::type>::type;
};

template<class U>
inline
constexpr
U
min_all(U u)
{
    BOOST_STATIC_ASSERT(std::is_unsigned<U>::value);
    return u;
}

template<class U0, class U1, class... UN>
inline
constexpr
typename unwidest_unsigned<U0, U1, UN...>::type
min_all(U0 u0, U1 u1, UN... un)
{
    using type =
        typename unwidest_unsigned<U0, U1, UN...>::type;
    return u0 < u1 ?
        static_cast<type>(min_all(u0, un...)) :
        static_cast<type>(min_all(u1, un...));
}

template<class U>
inline
constexpr
U
max_all(U u)
{
    BOOST_STATIC_ASSERT(std::is_unsigned<U>::value);
    return u;
}

template<class U0, class U1, class... UN>
inline
constexpr
typename widest_unsigned<U0, U1, UN...>::type
max_all(U0 u0, U1 u1, UN... un)
{
    return u0 > u1? max_all(u0, un...) : max_all(u1, un...);
}

//------------------------------------------------------------------------------

template<class T, class = void>
struct get_lowest_layer_helper
{
    using type = T;
};

template<class T>
struct get_lowest_layer_helper<T,
    void_t<typename T::lowest_layer_type>>
{
    using type = typename T::lowest_layer_type;
};

//------------------------------------------------------------------------------

//
// buffer concepts
//

// Types that meet the requirements,
// for use with std::declval only.
template<class BufferType>
struct BufferSequence
{
    using value_type = BufferType;
    using const_iterator = BufferType const*;
    ~BufferSequence();
    BufferSequence(BufferSequence const&) = default;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;
};
using ConstBufferSequence =
    BufferSequence<net::const_buffer>;
using MutableBufferSequence =
    BufferSequence<net::mutable_buffer>;

template<class B1, class... Bn>
struct is_all_const_buffer_sequence
    : std::integral_constant<bool,
        net::is_const_buffer_sequence<B1>::value &&
        is_all_const_buffer_sequence<Bn...>::value>
{
};

template<class B>
struct is_all_const_buffer_sequence<B>
    : net::is_const_buffer_sequence<B>
{
};

//
// Returns the iterator type of a buffer sequence
//

template<class B>
struct buffer_sequence_iterator
{
    using type = decltype(
        net::buffer_sequence_begin(
            std::declval<B const&>()));
};

template<>
struct buffer_sequence_iterator<
    net::const_buffer>
{
    using type = net::const_buffer const*;
};

template<>
struct buffer_sequence_iterator<
    net::mutable_buffer>
{
    using type = net::mutable_buffer const*;
};

//
// Returns the value type of the iterator of a buffer sequence
//

template<class B>
struct buffer_sequence_value_type
{
    using type = typename std::iterator_traits<
        typename buffer_sequence_iterator<B>::type
            >::value_type;
};

template<>
struct buffer_sequence_value_type<
    net::const_buffer const*>
{
    using type = net::const_buffer;
};

template<>
struct buffer_sequence_value_type<
    net::mutable_buffer const*>
{
    using type = net::mutable_buffer;
};

//

template<class... Bn>
struct common_buffers_type
{
    using type = typename std::conditional<
        mp11::mp_and<
            boost::is_convertible<
                typename buffer_sequence_value_type<Bn>::type,
                net::mutable_buffer>...>::value,
            net::mutable_buffer,
            net::const_buffer>::type;
};

// Types that meet the requirements,
// for use with std::declval only.
struct StreamHandler
{
    StreamHandler(StreamHandler const&) = default;
    void operator()(error_code ec, std::size_t);
};
using ReadHandler = StreamHandler;
using WriteHandler = StreamHandler;

/*  If this static assert goes off, it means that the completion
    handler you provided to an asynchronous initiating function did
    not have the right signature. Check the parameter types for your
    completion handler and make sure they match the list of types
    expected by the initiating function,
*/
#define BOOST_BEAST_HANDLER_INIT(type, sig) \
    static_assert(boost::beast::is_completion_handler< \
    BOOST_ASIO_HANDLER_TYPE(type, sig), sig>::value, \
    "CompletionHandler signature requirements not met"); \
    net::async_completion<type, sig> init{handler}

} // detail
} // beast
} // boost

#endif
