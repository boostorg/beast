//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_HEADER_PARSER_HPP
#define BEAST_HTTP_HEADER_PARSER_HPP

#include <beast/config.hpp>
#include <beast/http/message.hpp>
#include <beast/http/basic_parser.hpp>
#include <boost/throw_exception.hpp>
#include <array>
#include <type_traits>
#include <utility>

namespace beast {
namespace http {

/** A parser for producing HTTP/1 headers.

    This class uses the basic HTTP/1 wire format parser to convert
    a series of octets into a @ref header.

    @note A new instance of the parser is required for each message.

    @tparam isRequest Indicates whether a request or response
    will be parsed.

    @tparam Fields The type of container used to represent the fields.
*/
template<bool isRequest, class Fields>
class header_parser
    : public basic_parser<isRequest,
        header_parser<isRequest, Fields>>
{
    header<isRequest, Fields> h_;
    string_view body_;

public:
    /// The type of @ref header this object produces.
    using value_type = header<isRequest, Fields>;

    /// Default constructor.
    header_parser() = default;

    /// Copy constructor.
    header_parser(header_parser const&) = default;

    /// Copy assignment.
    header_parser& operator=(header_parser const&) = default;

    /** Move constructor.

        After the move, the only valid operation
        on the moved-from object is destruction.
    */
    header_parser(header_parser&&) = default;

    /** Move assignment

        After the move, the only valid operation
        on the moved-from object is destruction.
    */
    header_parser& operator=(header_parser&&) = default;

    /** Constructor

        @param args Optional arguments forwarded
        forwarded to the @ref http::header constructor.
    */
#if BEAST_DOXYGEN
    template<class... Args>
    explicit
    header_parser(Args&&... args);
#else
    template<class Arg0, class... ArgN,
        class = typename std::enable_if<
        ! std::is_convertible<typename
            std::decay<Arg0>::type,
                header_parser>::value>>
    explicit
    header_parser(Arg0&& arg0, ArgN&&... argn);
#endif

    /** Returns parsed body octets.

        This function will return the most recent buffer
        of octets corresponding to the parsed body. This
        buffer will become invalidated on any subsequent
        call to @ref put or @ref put_eof
    */
    string_view
    body() const
    {
        return body_;
    }

    /** Returns the parsed header

        @note The return value is undefined unless
        @ref is_header_done would return `true`.
    */
    value_type const&
    get() const
    {
        return h_;
    }

    /** Returns the parsed header.

        @note The return value is undefined unless
        @ref is_header_done would return `true`.
    */
    value_type&
    get()
    {
        return h_;
    }

    /** Returns ownership of the parsed header.

        Ownership is transferred to the caller.

        @note The return value is undefined unless
        @ref is_header_done would return `true`.

        Requires:
            @ref value_type is @b MoveConstructible
    */
    value_type
    release()
    {
        static_assert(
            std::is_move_constructible<decltype(h_)>::value,
            "MoveConstructible requirements not met");
        return std::move(h_);
    }

private:
    friend class basic_parser<isRequest, header_parser>;

    void
    on_request(string_view method,
        string_view path, int version, error_code&)
    {
        h_.target(path);
        h_.method(method);
        h_.version = version;
    }

    void
    on_response(int status, string_view reason,
        int version, error_code&)
    {
        h_.status = status;
        h_.version = version;
        h_.reason(reason);
    }

    void
    on_field(string_view name,
        string_view value, error_code&)
    {
        h_.fields.insert(name, value);
    }

    void
    on_header(error_code&)
    {
    }

    void
    on_body(boost::optional<std::
        uint64_t> const&, error_code&)
    {
    }

    void
    on_data(string_view s, error_code&)
    {
        body_ = s;
    }

    void
    on_chunk(std::uint64_t,
        string_view const&, error_code&)
    {
        body_ = {};
    }

    void
    on_complete(error_code&)
    {
        body_ = {};
    }
};

} // http
} // beast

#include <beast/http/impl/header_parser.ipp>

#endif
