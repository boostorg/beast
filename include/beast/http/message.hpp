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

/** A container for HTTP request headers.

    The container includes the headers, as well as the
    request method and URL. Objects of this type may be
    used to represent incoming or outgoing requests for
    which the body is not yet known or generated. For
    example, when receiving a request with the header value
    "Expect: 100-continue".
*/
template<class Headers>
struct request_headers
{
    /// Indicates if the message is a request.
    using is_request =
        std::integral_constant<bool, true>;

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

    /// The Request Method.
    std::string method;

    /// The Request URI.
    std::string url;

    /// The HTTP headers.
    Headers headers;

    /** Construct HTTP request headers.

        Arguments, if any, are forwarded to the constructor
        of the headers member.
    */
    /** @{ */
    request_headers() = default;

    template<class Arg1, class... ArgN,
        class = typename std::enable_if<
            (sizeof...(ArgN) > 0) || ! std::is_convertible<
                typename std::decay<Arg1>::type,
                    request_headers>::value>::type>
    explicit
    request_headers(Arg1&& arg1, ArgN&&... argn)
        : headers(std::forward<Arg1>(arg1),
            std::forward<ArgN>(argn)...)
    {
    }
    /** @} */
};

/** Swap two HTTP request headers.

    Requirements:

        Headers is Swappable.
*/
template<class Headers>
void
swap(
    request_headers<Headers>& a,
    request_headers<Headers>& b)
{
    using std::swap;
    swap(a.version, b.version);
    swap(a.method, b.method);
    swap(a.url, b.url);
    swap(a.headers, b.headers);
}

/** A container for HTTP response headers.

    The container includes the headers, as well as the
    response status and reasons. Objects of this type may
    be used to represent incoming or outgoing responses for
    which the body is not yet known or generated. For
    example, when responding to a HEAD request.
*/
template<class Headers>
struct response_headers
{
    /// Indicates if the message is a response.
    using is_request =
        std::integral_constant<bool, false>;

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

    /// The Response Status-Code.
    int status;

    /** The Response Reason-Phrase.

        The Reason-Phrase is obsolete as of rfc7230.
    */
    std::string reason;

    /// The HTTP headers.
    Headers headers;

    /** Construct HTTP request headers.

        Arguments, if any, are forwarded to the constructor
        of the headers member.
    */
    /** @{ */
    response_headers() = default;

    template<class Arg1, class... ArgN,
        class = typename std::enable_if<
            (sizeof...(ArgN) > 0) || ! std::is_convertible<
                typename std::decay<Arg1>::type,
                    response_headers>::value>::type>
    explicit
    response_headers(Arg1&& arg1, ArgN&&... argn)
        : headers(std::forward<Arg1>(arg1),
            std::forward<ArgN>(argn)...)
    {
    }
    /** @} */
};

/** Swap two HTTP response headers.

    Requirements:

        Headers is Swappable.
*/
template<class Headers>
void
swap(
    response_headers<Headers>& a,
    response_headers<Headers>& b)
{
    using std::swap;
    swap(a.version, b.version);
    swap(a.status, b.status);
    swap(a.reason, b.reason);
    swap(a.headers, b.headers);
}

/** A container for HTTP request or response headers.
*/
#if GENERATING_DOCS
template<bool isRequest, class Headers>
struct message_headers
{
    /// Indicates if the message is a request.
    using is_request =
        std::integral_constant<bool, isRequest>;

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

    /** The Request Method.

        @note This field is present only if `isRequest == true`.
    */
    std::string method;

    /** The Request-URI.

        @note This field is present only if `isRequest == true`.
    */
    std::string url;

    /** The Response Status-Code.

        @note This field is present only if `isRequest == false`.
    */
    int status;

    /** The Response Reason-Phrase.

        The Reason-Phrase is obsolete as of rfc7230.

         @note This field is present only if `isRequest == false`.
    */
    std::string reason;

    /// The HTTP headers.
    Headers headers;

    /** Construct message headers.

        Any provided arguments are forwarded to the
        constructor of the headers member.
    */
    template<class... Args>
    message_headers(Args&&... args);
};

#else
template<bool isRequest, class Headers>
using message_headers =
    typename std::conditional<isRequest,
        request_headers<Headers>,
        response_headers<Headers>>::type;

#endif

/** A complete HTTP message.

    A message can be a request or response, depending on the `isRequest`
    template argument value. Requests and responses have different types,
    so functions may be overloaded on them if desired.

    The `Body` template argument type determines the model used
    to read or write the content body of the message.

    @tparam isRequest `true` if this is a request.

    @tparam Body A type meeting the requirements of Body.

    @tparam Headers A type meeting the requirements of Headers.
*/
template<bool isRequest, class Body, class Headers>
struct message :
#if GENERATING_DOCS
    implementation_defined
#else
    message_headers<isRequest, Headers>
#endif
{
#if GENERATING_DOCS
    /// Indicates if the message is a request.
    using is_request =
        std::integral_constant<bool, isRequest>;

    /// The type representing the headers.
    using headers_type = Headers;

    /** The type controlling the body traits.

        The body member will be of type `body_type::value_type`.
    */
    using body_type = Body;

    /** The HTTP version.

        This holds both the major and minor version numbers,
        using these formulas:
        @code
            major = version / 10;
            minor = version % 10;
        @endcode
    */
    int version;

    /** The Request Method.

        @note This field is present only if `isRequest == true`.
    */
    std::string method;

    /** The Request-URI.

        @note This field is present only if `isRequest == true`.
    */
    std::string url;

    /** The Response Status-Code.

        @note This field is present only if `isRequest == false`.
    */
    int status;

    /** The Response Reason-Phrase.

        The Reason-Phrase is obsolete as of rfc7230.

         @note This field is present only if `isRequest == false`.
    */
    std::string reason;

    /// The HTTP headers.
    Headers headers;

#else
    /// The container used to hold the request or response headers
    using base_type = message_headers<isRequest, Headers>;

    /** The type controlling the body traits.

        The body member will be of type `body_type::value_type`.
    */
    using body_type = Body;

#endif

    /// A container representing the body.
    typename Body::value_type body;

    /// Default constructor
    message() = default;

    /** Construct a message from headers.

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

    /** Construct a message from headers.

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

/// Swap one message for another message.
template<bool isRequest, class Body, class Headers>
void
swap(
    message<isRequest, Body, Headers>& a,
    message<isRequest, Body, Headers>& b)
{
    using std::swap;
    using base_type = typename message<
        isRequest, Body, Headers>::base_type;
    swap(static_cast<base_type&>(a),
        static_cast<base_type&>(b));
    swap(a.body, b.body);
}

/// A typical HTTP request
template<class Body,
    class Headers = basic_headers<std::allocator<char>>>
using request = message<true, Body, Headers>;

/// A typical HTTP response
template<class Body,
    class Headers = basic_headers<std::allocator<char>>>
using response = message<false, Body, Headers>;

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
