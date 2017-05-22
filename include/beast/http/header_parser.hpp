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
    : public basic_parser<isRequest, false,
        header_parser<isRequest, Fields>>
{
    header<isRequest, Fields> h_;

public:
    using mutable_buffers_type =
        boost::asio::null_buffers;

    /// The type of @ref header this object produces.
    using value_type = header<isRequest, Fields>;

    /// Copy constructor.
    header_parser(header_parser const&) = default;

    /// Copy assignment.
    header_parser& operator=(header_parser const&) = default;

    /** Move constructor.

        After the move, the only valid operation
        on the moved-from object is destruction.
    */
    header_parser(header_parser&&) = default;

    /** Constructor

        @param args If present, additional arguments to be
        forwarded to the @ref beast::http::header constructor.
    */
    template<class... Args>
    explicit
    header_parser(Args&&... args);

    /** Returns the parsed header

        Only valid if @ref got_header would return `true`.
    */
    value_type const&
    get() const
    {
        return h_;
    }

    /** Returns the parsed header.

        Only valid if @ref got_header would return `true`.
    */
    value_type&
    get()
    {
        return h_;
    }

    /** Returns ownership of the parsed header.

        Ownership is transferred to the caller. Only
        valid if @ref got_header would return `true`.

        Requires:
            @ref value_type is @b MoveConstructible
    */
    value_type
    release()
    {
        static_assert(std::is_move_constructible<decltype(h_)>::value,
            "MoveConstructible requirements not met");
        return std::move(h_);
    }

private:
    friend class basic_parser<
        isRequest, false, header_parser>;

    void
    on_request(
        string_view const& method,
            string_view const& path,
                int version, error_code&)
    {
        h_.target(path);
        h_.method(method);
        h_.version = version;
    }

    void
    on_response(int status,
        string_view const& reason,
            int version, error_code&)
    {
        h_.status = status;
        h_.version = version;
        h_.reason(reason);
    }

    void
    on_field(string_view const& name,
        string_view const& value,
            error_code&)
    {
        h_.fields.insert(name, value);
    }

    void
    on_header(error_code&)
    {
    }

    void
    on_body(error_code& ec)
    {
    }

    void
    on_body(std::uint64_t content_length,
        error_code& ec)
    {
    }

    void
    on_data(string_view const& s,
        error_code& ec)
    {
    }

    void
    on_commit(std::size_t n)
    {
        // Can't write body data with header-only parser!
        BOOST_ASSERT(false);
        BOOST_THROW_EXCEPTION(std::logic_error{
            "invalid member function call"});
    }

    void
    on_chunk(std::uint64_t n,
        string_view const& ext,
            error_code& ec)
    {
    }

    void
    on_complete(error_code&)
    {
    }
};

} // http
} // beast

#include <beast/http/impl/header_parser.ipp>

#endif
