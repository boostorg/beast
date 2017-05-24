//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_DETAIL_TYPE_TRAITS_HPP
#define BEAST_DETAIL_TYPE_TRAITS_HPP

#include <beast/core/error.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_service.hpp>
#include <iterator>
#include <tuple>
#include <type_traits>
#include <string>

namespace beast {
namespace detail {

//
// utilities
//

template<class... Ts>
struct make_void
{
    using type = void;
};

template<class... Ts>
using void_t = typename make_void<Ts...>::type;

template<class... Ts>
inline
void
ignore_unused(Ts const& ...)
{
}

template<class... Ts>
inline
void
ignore_unused()
{}

template<class U>
std::size_t constexpr
max_sizeof()
{
    return sizeof(U);
}

template<class U0, class U1, class... Us>
std::size_t constexpr
max_sizeof()
{
    return
        max_sizeof<U0>() > max_sizeof<U1, Us...>() ?
        max_sizeof<U0>() : max_sizeof<U1, Us...>();
}

template<unsigned N, class T, class... Tn>
struct repeat_tuple_impl
{
    using type = typename repeat_tuple_impl<
        N - 1, T, T, Tn...>::type;
};

template<class T, class... Tn>
struct repeat_tuple_impl<0, T, Tn...>
{
    using type = std::tuple<T, Tn...>;
};

template<unsigned N, class T>
struct repeat_tuple
{
    using type =
        typename repeat_tuple_impl<N-1, T>::type;
};

template<class T>
struct repeat_tuple<0, T>
{
    using type = std::tuple<>;
};

template<class R, class C, class ...A>
auto
is_invocable_test(C&& c, int, A&& ...a)
    -> decltype(std::is_convertible<
        decltype(c(a...)), R>::value ||
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
struct is_invocable
    : std::false_type
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
    BufferSequence<boost::asio::const_buffer>;
using MutableBufferSequence =
    BufferSequence<boost::asio::mutable_buffer>;

template<class T, class BufferType>
class is_buffer_sequence
{
    template<class U, class R = std::is_convertible<
        typename U::value_type, BufferType> >
    static R check1(int);
    template<class>
    static std::false_type check1(...);
    using type1 = decltype(check1<T>(0));

    template<class U, class R = std::is_base_of<
    #if 0
        std::bidirectional_iterator_tag,
            typename std::iterator_traits<
                typename U::const_iterator>::iterator_category>>
    #else
        // workaround:
        // boost::asio::detail::consuming_buffers::const_iterator
        // is not bidirectional
        std::forward_iterator_tag,
            typename std::iterator_traits<
                typename U::const_iterator>::iterator_category>>
    #endif
    static R check2(int);
    template<class>
    static std::false_type check2(...);
    using type2 = decltype(check2<T>(0));

    template<class U, class R = typename
        std::is_convertible<decltype(
            std::declval<U>().begin()),
                typename U::const_iterator>::type>
    static R check3(int);
    template<class>
    static std::false_type check3(...);
    using type3 = decltype(check3<T>(0));

    template<class U, class R = typename std::is_convertible<decltype(
        std::declval<U>().end()),
            typename U::const_iterator>::type>
    static R check4(int);
    template<class>
    static std::false_type check4(...);
    using type4 = decltype(check4<T>(0));

public:
    using type = std::integral_constant<bool,
        std::is_copy_constructible<T>::value &&
        std::is_destructible<T>::value &&
        type1::value && type2::value &&
        type3::value && type4::value>;
};

//------------------------------------------------------------------------------

template<class B1, class... Bn>
struct is_all_const_buffer_sequence
    : std::integral_constant<bool,
        is_buffer_sequence<B1, boost::asio::const_buffer>::type::value &&
        is_all_const_buffer_sequence<Bn...>::value>
{
};

template<class B1>
struct is_all_const_buffer_sequence<B1>
    : is_buffer_sequence<B1, boost::asio::const_buffer>::type
{
};

template<class... Bn>
struct common_buffers_type
{
    using type = typename std::conditional<
        std::is_convertible<std::tuple<Bn...>,
            typename repeat_tuple<sizeof...(Bn),
                boost::asio::mutable_buffer>::type>::value,
                    boost::asio::mutable_buffer,
                        boost::asio::const_buffer>::type;
};

//------------------------------------------------------------------------------

//
// stream concepts
//

// Types that meet the requirements,
// for use with std::declval only.
struct StreamHandler
{
    StreamHandler(StreamHandler const&) = default;
    void operator()(error_code ec, std::size_t);
};
using ReadHandler = StreamHandler;
using WriteHandler = StreamHandler;

template<class T>
class has_get_io_service
{
    template<class U, class R = typename std::is_same<
        decltype(std::declval<U>().get_io_service()),
            boost::asio::io_service&>>
    static R check(int);
    template<class>
    static std::false_type check(...);
public:
    using type = decltype(check<T>(0));
};

template<class T>
class is_async_read_stream
{
    template<class U, class R = decltype(
        std::declval<U>().async_read_some(
            std::declval<MutableBufferSequence>(),
                std::declval<ReadHandler>()),
                    std::true_type{})>
    static R check(int);
    template<class>
    static std::false_type check(...);
    using type1 = decltype(check<T>(0));
public:
    using type = std::integral_constant<bool,
        type1::value &&
        has_get_io_service<T>::type::value>;
};

template<class T>
class is_async_write_stream
{
    template<class U, class R = decltype(
        std::declval<U>().async_write_some(
            std::declval<ConstBufferSequence>(),
                std::declval<WriteHandler>()),
                    std::true_type{})>
    static R check(int);
    template<class>
    static std::false_type check(...);
    using type1 = decltype(check<T>(0));
public:
    using type = std::integral_constant<bool,
        type1::value &&
        has_get_io_service<T>::type::value>;
};

template<class T>
class is_sync_read_stream
{
    template<class U, class R = std::is_same<decltype(
        std::declval<U>().read_some(
            std::declval<MutableBufferSequence>())),
                std::size_t>>
    static R check1(int);
    template<class>
    static std::false_type check1(...);
    using type1 = decltype(check1<T>(0));

    template<class U, class R = std::is_same<decltype(
        std::declval<U>().read_some(
            std::declval<MutableBufferSequence>(),
                std::declval<error_code&>())), std::size_t>>
    static R check2(int);
    template<class>
    static std::false_type check2(...);
    using type2 = decltype(check2<T>(0));

public:
    using type = std::integral_constant<bool,
        type1::value && type2::value>;
};

template<class T>
class is_sync_write_stream
{
    template<class U, class R = std::is_same<decltype(
        std::declval<U>().write_some(
            std::declval<ConstBufferSequence>())),
                std::size_t>>
    static R check1(int);
    template<class>
    static std::false_type check1(...);
    using type1 = decltype(check1<T>(0));

    template<class U, class R = std::is_same<decltype(
        std::declval<U>().write_some(
            std::declval<ConstBufferSequence>(),
                std::declval<error_code&>())), std::size_t>>
    static R check2(int);
    template<class>
    static std::false_type check2(...);
    using type2 = decltype(check2<T>(0));

public:
    using type = std::integral_constant<bool,
        type1::value && type2::value>;
};

} // detail
} // beast

#endif
