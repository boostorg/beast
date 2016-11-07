//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_MESSAGE_HPP
#define BEAST_HTTP_MESSAGE_HPP

#include <beast/http/basic_headers.hpp>
#include <beast/core/detail/integer_sequence.hpp>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

namespace beast {
namespace http {

#if GENERATING_DOCS
/** A container for HTTP request or response headers.

    The container includes the headers, as well as the
    request method and URL. Objects of this type may be
    used to represent incoming or outgoing requests for
    which the body is not yet known or generated. For
    example, when receiving a request with the header value
    "Expect: 100-continue".
*/
template<bool isRequest, class Headers>
struct message_headers

#else
template<bool isRequest, class Headers>
struct message_headers;

template<class Headers>
struct message_headers<true, Headers>
#endif
{
    /// Indicates if the message headers are a request or response.
#if GENERATING_DOCS
    static bool constexpr is_request = isRequest;

#else
    static bool constexpr is_request = true;
#endif

    /// The type representing the headers.
    using headers_type = Headers;

    /** The HTTP version.

        This holds both the major and minor version numbers,
        using these formulas:
        @code
            major = version / 10;
            minor = version % 10;
        @endcode
    */
    int version;

    /** The Request Method

        @note This field is present only if `isRequest == true`.
    */
    std::string method;

    /** The Request URI

        @note This field is present only if `isRequest == true`.
    */
    std::string url;

    /// The HTTP field values.
    Headers headers;

    /// Default constructor
    message_headers() = default;

    /// Move constructor
    message_headers(message_headers&&) = default;

    /// Copy constructor
    message_headers(message_headers const&) = default;

    /// Move assignment
    message_headers& operator=(message_headers&&) = default;

    /// Copy assignment
    message_headers& operator=(message_headers const&) = default;

    /** Construct message headers.

        All arguments are forwarded to the constructor
        of the `headers` member.

        @note This constructor participates in overload resolution
        if and only if the first parameter is not convertible to
        `message_headers`.
    */
#if GENERATING_DOCS
    template<class... Args>
    explicit
    message_headers(Args&&... args);

#else
    template<class Arg1, class... ArgN,
        class = typename std::enable_if<
            (sizeof...(ArgN) > 0) || ! std::is_convertible<
                typename std::decay<Arg1>::type,
                    message_headers>::value>::type>
    explicit
    message_headers(Arg1&& arg1, ArgN&&... argn)
        : headers(std::forward<Arg1>(arg1),
            std::forward<ArgN>(argn)...)
    {
    }
};

/** A container for HTTP request or response headers.

    The container includes the headers, as well as the
    response status and reasons. Objects of this type may
    be used to represent incoming or outgoing responses for
    which the body is not yet known or generated. For
    example, when responding to a HEAD request.
*/
template<class Headers>
struct message_headers<false, Headers>
{
    /// Indicates if the message headers are a request or response.
    static bool constexpr is_request = false;

    /// The type representing the headers.
    using headers_type = Headers;

    /** The HTTP version.

        This holds both the major and minor version numbers,
        using these formulas:
        @code
            major = version / 10;
            minor = version % 10;
        @endcode
    */
    int version;

    /// The HTTP field values.
    Headers headers;

    /// Default constructor
    message_headers() = default;

    /// Move constructor
    message_headers(message_headers&&) = default;

    /// Copy constructor
    message_headers(message_headers const&) = default;

    /// Move assignment
    message_headers& operator=(message_headers&&) = default;

    /// Copy assignment
    message_headers& operator=(message_headers const&) = default;

    /** Construct message headers.

        All arguments are forwarded to the constructor
        of the `headers` member.

        @note This constructor participates in overload resolution
        if and only if the first parameter is not convertible to
        `message_headers`.
    */
    template<class Arg1, class... ArgN,
        class = typename std::enable_if<
            (sizeof...(ArgN) > 0) || ! std::is_convertible<
                typename std::decay<Arg1>::type,
                    message_headers>::value>::type>
    explicit
    message_headers(Arg1&& arg1, ArgN&&... argn)
        : headers(std::forward<Arg1>(arg1),
            std::forward<ArgN>(argn)...)
    {
    }
#endif

    /** The Response Status-Code.

        @note This field is present only if `isRequest == false`.
    */
    int status;

    /** The Response Reason-Phrase.

        The Reason-Phrase is obsolete as of rfc7230.

        @note This field is present only if `isRequest == false`.
    */
    std::string reason;
};

/** A container for a complete HTTP message.

    A message can be a request or response, depending on the `isRequest`
    template argument value. Requests and responses have different types,
    so functions may be overloaded on them if desired.

    The `Body` template argument type determines the model used
    to read or write the content body of the message.

    @tparam isRequest `true` if this represents a request,
    or `false` if this represents a response. Some class data
    members are conditionally present depending on this value.

    @tparam Body A type meeting the requirements of Body.

    @tparam Headers The type of container used to hold the
    field value pairs.
*/
template<bool isRequest, class Body, class Headers>
struct message :
    message_headers<isRequest, Headers>
{
    /// The base class used to hold the request or response headers
    using base_type = message_headers<isRequest, Headers>;

    /** The type providing the body traits.

        The `body` member will be of type `body_type::value_type`.
    */
    using body_type = Body;

    /// A value representing the body.
    typename Body::value_type body;

    /// Default constructor
    message() = default;

    /** Construct a message from message headers.

        Additional arguments, if any, are forwarded to
        the constructor of the body member.
    */
    template<class... Args>
    explicit
    message(base_type&& base, Args&&... args)
        : base_type(std::move(base))
        , body(std::forward<Args>(args)...)
    {
    }

    /** Construct a message from message headers.

        Additional arguments, if any, are forwarded to
        the constructor of the body member.
    */
    template<class... Args>
    explicit
    message(base_type const& base, Args&&... args)
        : base_type(base)
        , body(std::forward<Args>(args)...)
    {
    }

    /** Construct a message.

        @param u An argument forwarded to the body constructor.

        @note This constructor participates in overload resolution
        only if `u` is not convertible to `base_type`.
    */
    template<class U
#if ! GENERATING_DOCS
        , class = typename std::enable_if<! std::is_convertible<
            typename std::decay<U>::type, base_type>::value>
#endif
    >
    explicit
    message(U&& u)
        : body(std::forward<U>(u))
    {
    }

    /** Construct a message.

        @param u An argument forwarded to the body constructor.

        @param v An argument forwarded to the headers constructor.

        @note This constructor participates in overload resolution
        only if `u` is not convertible to `base_type`.
    */
    template<class U, class V
#if ! GENERATING_DOCS
        ,class = typename std::enable_if<! std::is_convertible<
            typename std::decay<U>::type, base_type>::value>
#endif
    >
    message(U&& u, V&& v)
        : base_type(std::forward<V>(v))
        , body(std::forward<U>(u))
    {
    }

    /** Construct a message.

        @param un A tuple forwarded as a parameter pack to the body constructor.
    */
    template<class... Un>
    message(std::piecewise_construct_t, std::tuple<Un...> un)
        : message(std::piecewise_construct, un,
            beast::detail::make_index_sequence<sizeof...(Un)>{})
    {
    }

    /** Construct a message.

        @param un A tuple forwarded as a parameter pack to the body constructor.

        @param vn A tuple forwarded as a parameter pack to the headers constructor.
    */
    template<class... Un, class... Vn>
    message(std::piecewise_construct_t,
            std::tuple<Un...>&& un, std::tuple<Vn...>&& vn)
        : message(std::piecewise_construct, un, vn,
            beast::detail::make_index_sequence<sizeof...(Un)>{},
            beast::detail::make_index_sequence<sizeof...(Vn)>{})
    {
    }

    /// Returns the message headers portion of the message
    base_type&
    base()
    {
        return *this;
    }

    /// Returns the message headers portion of the message
    base_type const&
    base() const
    {
        return *this;
    }

private:
    template<class... Un, size_t... IUn>
    message(std::piecewise_construct_t,
            std::tuple<Un...>& tu, beast::detail::index_sequence<IUn...>)
        : body(std::forward<Un>(std::get<IUn>(tu))...)
    {
    }

    template<class... Un, class... Vn,
        std::size_t... IUn, std::size_t... IVn>
    message(std::piecewise_construct_t,
            std::tuple<Un...>& tu, std::tuple<Vn...>& tv,
                beast::detail::index_sequence<IUn...>,
                    beast::detail::index_sequence<IVn...>)
        : base_type(std::forward<Vn>(std::get<IVn>(tv))...)
        , body(std::forward<Un>(std::get<IUn>(tu))...)
    {
    }
};

//------------------------------------------------------------------------------

#if GENERATING_DOCS
/** Swap two HTTP message headers.

    @par Requirements
    `Headers` is @b Swappable.
*/
template<bool isRequest, class Headers>
void
swap(
    message_headers<isRequest, Headers>& m1,
    message_headers<isRequest, Headers>& m2);
#endif

/** Swap two HTTP messages.

    @par Requirements:
    `Body` and `Headers` are @b Swappable.
*/
template<bool isRequest, class Body, class Headers>
void
swap(
    message<isRequest, Body, Headers>& m1,
    message<isRequest, Body, Headers>& m2);

/// Message headers for a typical HTTP request
using request_headers = message_headers<true,
    basic_headers<std::allocator<char>>>;

/// Message headers for a typical HTTP response
using response_headers = message_headers<false,
    basic_headers<std::allocator<char>>>;

/// A typical HTTP request message
template<class Body,
    class Headers = basic_headers<std::allocator<char>>>
using request = message<true, Body, Headers>;

/// A typical HTTP response message
template<class Body,
    class Headers = basic_headers<std::allocator<char>>>
using response = message<false, Body, Headers>;

//------------------------------------------------------------------------------

/** Returns `true` if a HTTP/1 message indicates a keep alive.

    Undefined behavior if version is greater than 11.
*/
template<bool isRequest, class Body, class Headers>
bool
is_keep_alive(message<isRequest, Body, Headers> const& msg);

/** Returns `true` if a HTTP/1 message indicates an Upgrade request or response.

    Undefined behavior if version is greater than 11.
*/
template<bool isRequest, class Body, class Headers>
bool
is_upgrade(message<isRequest, Body, Headers> const& msg);

/** HTTP/1 connection prepare options.

    @note These values are used with @ref prepare.
*/
enum class connection
{
    /// Specify Connection: close.
    close,

    /// Specify Connection: keep-alive where possible.
    keep_alive,

    /// Specify Connection: upgrade.
    upgrade
};

/** Prepare a HTTP message.

    This function will adjust the Content-Length, Transfer-Encoding,
    and Connection headers of the message based on the properties of
    the body and the options passed in.

    @param msg The message to prepare. The headers may be modified.

    @param options A list of prepare options.
*/
template<
    bool isRequest, class Body, class Headers,
    class... Options>
void
prepare(message<isRequest, Body, Headers>& msg,
    Options&&... options);

} // http
} // beast

#include <beast/http/impl/message.ipp>

#endif
