//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_BASIC_PARSER_IPP
#define BEAST_HTTP_IMPL_BASIC_PARSER_IPP

#include <beast/core/buffer_concepts.hpp>
#include <beast/core/detail/ci_char_traits.hpp>
#include <beast/core/detail/clamp.hpp>
#include <beast/core/detail/type_traits.hpp>
#include <beast/http/error.hpp>
#include <beast/http/rfc7230.hpp>
#include <boost/asio/buffer.hpp>
#include <algorithm>
#include <utility>

namespace beast {
namespace http {

template<bool isRequest, bool isDirect, class Derived>
template<bool OtherIsDirect, class OtherDerived>
basic_parser<isRequest, isDirect, Derived>::
basic_parser(basic_parser<isRequest,
        OtherIsDirect, OtherDerived>&& other)
    : len_(other.len_)
    , buf_(std::move(other.buf_))
    , buf_len_(other.buf_len_)
    , skip_(other.skip_)
    , x_(other.x_)
    , f_(other.f_)
    , state_(other.state_)
{
}

template<bool isRequest, bool isDirect, class Derived>
void
basic_parser<isRequest, isDirect, Derived>::
skip_body()
{
    BOOST_ASSERT(! got_some());
    f_ |= flagSkipBody;
}

template<bool isRequest, bool isDirect, class Derived>
bool
basic_parser<isRequest, isDirect, Derived>::
is_keep_alive() const
{
    BOOST_ASSERT(got_header());
    if(f_ & flagHTTP11)
    {
        if(f_ & flagConnectionClose)
            return false;
    }
    else
    {
        if(! (f_ & flagConnectionKeepAlive))
            return false;
    }
    return (f_ & flagNeedEOF) == 0;
}

template<bool isRequest, bool isDirect, class Derived>
template<class ConstBufferSequence>
std::size_t
basic_parser<isRequest, isDirect, Derived>::
write(ConstBufferSequence const& buffers,
    error_code& ec)
{
    static_assert(is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    auto const buffer = maybe_flatten(buffers);
    return write(boost::asio::const_buffers_1{
        buffer.data(), buffer.size()}, ec);
}

template<bool isRequest, bool isDirect, class Derived>
std::size_t
basic_parser<isRequest, isDirect, Derived>::
write(boost::asio::const_buffers_1 const& buffer,
    error_code& ec)
{
    return do_write(buffer, ec,
        std::integral_constant<bool, isDirect>{});
}

template<bool isRequest, bool isDirect, class Derived>
void
basic_parser<isRequest, isDirect, Derived>::
write_eof(error_code& ec)
{
    BOOST_ASSERT(got_some());
    if(state_ == parse_state::header)
    {
        ec = error::partial_message;
        return;
    }
    if(f_ & (flagContentLength | flagChunked))
    {
        if(state_ != parse_state::complete)
        {
            ec = error::partial_message;
            return;
        }
        return;
    }
    do_complete(ec);
    if(ec)
        return;
}

template<bool isRequest, bool isDirect, class Derived>
template<class DynamicBuffer>
std::size_t
basic_parser<isRequest, isDirect, Derived>::
copy_body(DynamicBuffer& dynabuf)
{
    // This function not available when isDirect==false
    static_assert(isDirect, "");

    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    BOOST_ASSERT(dynabuf.size() > 0);
    BOOST_ASSERT(
        state_ == parse_state::body ||
        state_ == parse_state::body_to_eof ||
        state_ == parse_state::chunk_body);
    maybe_do_body_direct();
    switch(state_)
    {
    case parse_state::body_to_eof:
    {
        auto const buffers =
            impl().on_prepare(dynabuf.size());
        BOOST_ASSERT(
            buffer_size(buffers) >= 1 &&
            buffer_size(buffers) <=
                dynabuf.size());
        auto const n = buffer_copy(
            buffers, dynabuf.data());
        dynabuf.consume(n);
        impl().on_commit(n);
        return n;
    }

    default:
    {
        BOOST_ASSERT(len_ > 0);
        auto const buffers =
            impl().on_prepare(
                beast::detail::clamp(len_));
        BOOST_ASSERT(
            buffer_size(buffers) >= 1 &&
            buffer_size(buffers) <=
                beast::detail::clamp(len_));
        auto const n = buffer_copy(
            buffers, dynabuf.data());
        commit_body(n);
        return n;
    }
    }
}

template<bool isRequest, bool isDirect, class Derived>
template<class MutableBufferSequence>
void
basic_parser<isRequest, isDirect, Derived>::
prepare_body(boost::optional<
    MutableBufferSequence>& buffers, std::size_t limit)
{
    // This function not available when isDirect==false
    static_assert(isDirect, "");

    BOOST_ASSERT(limit > 0);
    BOOST_ASSERT(
        state_ == parse_state::body ||
        state_ == parse_state::body_to_eof ||
        state_ == parse_state::chunk_body);
    maybe_do_body_direct();
    std::size_t n;
    switch(state_)
    {
    case parse_state::body_to_eof:
        n = limit;
        break;

    default:
        BOOST_ASSERT(len_ > 0);
        n = beast::detail::clamp(len_, limit);
        break;
    }
    buffers.emplace(impl().on_prepare(n));
}

template<bool isRequest, bool isDirect, class Derived>
void
basic_parser<isRequest, isDirect, Derived>::
commit_body(std::size_t n)
{
    // This function not available when isDirect==false
    static_assert(isDirect, "");

    BOOST_ASSERT(f_ & flagOnBody);
    impl().on_commit(n);
    switch(state_)
    {
    case parse_state::body:
        len_ -= n;
        if(len_ == 0)
        {
            // VFALCO This is no good, throwing out ec?
            error_code ec;
            do_complete(ec);
        }
        break;

    case parse_state::chunk_body:
        len_ -= n;
        if(len_ == 0)
            state_ = parse_state::chunk_header;
        break;
    
    default:
        break;
    }
}

template<bool isRequest, bool isDirect, class Derived>
template<class ConstBufferSequence>
inline
string_view
basic_parser<isRequest, isDirect, Derived>::
maybe_flatten(
    ConstBufferSequence const& buffers)
{
    using boost::asio::buffer;
    using boost::asio::buffer_cast;
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;

    auto const it = buffers.begin();
    auto const last = buffers.end();
    if(it == last)
        return {nullptr, 0};
    if(std::next(it) == last)
    {
        // single buffer
        auto const b = *it;
        return {buffer_cast<char const*>(b),
            buffer_size(b)};
    }
    auto const len = buffer_size(buffers);
    if(len > buf_len_)
    {
        // reallocate
        buf_.reset(new char[len]);
        buf_len_ = len;
    }
    // flatten
    buffer_copy(
        buffer(buf_.get(), buf_len_), buffers);
    return {buf_.get(), buf_len_};
}

template<bool isRequest, bool isDirect, class Derived>
inline
std::size_t
basic_parser<isRequest, isDirect, Derived>::
do_write(boost::asio::const_buffers_1 const& buffer,
    error_code& ec, std::true_type)
{
    BOOST_ASSERT(
        state_ == parse_state::header ||
        state_ == parse_state::chunk_header);
    using boost::asio::buffer_cast;
    using boost::asio::buffer_size;
    auto const p =  buffer_cast<
        char const*>(*buffer.begin());
    auto const n =
        buffer_size(*buffer.begin());
    if(state_ == parse_state::header)
    {
        if(n > 0)
            f_ |= flagGotSome;
        return parse_header(p, n, ec);
    }
    else
    {
        maybe_do_body_direct();
        return parse_chunk_header(p, n, ec);
    }
}

template<bool isRequest, bool isDirect, class Derived>
inline
std::size_t
basic_parser<isRequest, isDirect, Derived>::
do_write(boost::asio::const_buffers_1 const& buffer,
    error_code& ec, std::false_type)
{
    BOOST_ASSERT(state_ != parse_state::complete);
    using boost::asio::buffer_cast;
    using boost::asio::buffer_size;
    auto const p =  buffer_cast<
        char const*>(*buffer.begin());
    auto const n =
        buffer_size(*buffer.begin());
    switch(state_)
    {
    case parse_state::header:
        if(n > 0)
            f_ |= flagGotSome;
        return parse_header(p, n, ec);

    case parse_state::body:
        maybe_do_body_indirect(ec);
        if(ec)
            return 0;
        return parse_body(p, n, ec);

    case parse_state::body_to_eof:
        maybe_do_body_indirect(ec);
        if(ec)
            return 0;
        return parse_body_to_eof(p, n, ec);

    case parse_state::chunk_header:
        maybe_do_body_indirect(ec);
        if(ec)
            return 0;
        return parse_chunk_header(p, n, ec);

    case parse_state::chunk_body:
        return parse_chunk_body(p, n, ec);

    case parse_state::complete:
        break;
    }
    return 0;
}


template<bool isRequest, bool isDirect, class Derived>
void
basic_parser<isRequest, isDirect, Derived>::
parse_startline(char const*& it,
    int& version, int& status,
        error_code& ec, std::true_type)
{
/*
    request-line   = method SP request-target SP HTTP-version CRLF
    method         = token
*/
    auto const method = parse_method(it);
    if(method.empty())
    {
        ec = error::bad_method;
        return;
    }
    if(*it++ != ' ')
    {
        ec = error::bad_method;
        return;
    }

    auto const target = parse_target(it);
    if(target.empty())
    {
        ec = error::bad_path;
        return;
    }
    if(*it++ != ' ')
    {
        ec = error::bad_path;
        return;
    }

    version = parse_version(it);
    if(version < 0 || ! parse_crlf(it))
    {
        ec = error::bad_version;
        return;
    }

    impl().on_request(
        method, target, version, ec);
    if(ec)
        return;
}

template<bool isRequest, bool isDirect, class Derived>
void
basic_parser<isRequest, isDirect, Derived>::
parse_startline(char const*& it,
    int& version, int& status,
        error_code& ec, std::false_type)
{
/*
     status-line    = HTTP-version SP status-code SP reason-phrase CRLF
     status-code    = 3*DIGIT
     reason-phrase  = *( HTAB / SP / VCHAR / obs-text )
*/
    version = parse_version(it);
    if(version < 0 || *it != ' ')
    {
        ec = error::bad_version;
        return;
    }
    ++it;

    status = parse_status(it);
    if(status < 0 || *it != ' ')
    {
        ec = error::bad_status;
        return;
    }
    ++it;

    auto const reason = parse_reason(it);
    if(! parse_crlf(it))
    {
        ec = error::bad_reason;
        return;
    }

    impl().on_response(
        status, reason, version, ec);
    if(ec)
        return;
}

template<bool isRequest, bool isDirect, class Derived>
void
basic_parser<isRequest, isDirect, Derived>::
parse_fields(char const*& it,
    char const* last, error_code& ec)
{
/*  header-field   = field-name ":" OWS field-value OWS

    field-name     = token
    field-value    = *( field-content / obs-fold )
    field-content  = field-vchar [ 1*( SP / HTAB ) field-vchar ]
    field-vchar    = VCHAR / obs-text

    obs-fold       = CRLF 1*( SP / HTAB )
                   ; obsolete line folding
                   ; see Section 3.2.4
*/
    for(;;)
    {
        auto term = find_eol(it, last, ec);
        if(ec)
            return;
        BOOST_ASSERT(term);
        if(it == term - 2)
        {
            it = term;
            break;
        }
        auto const name = parse_name(it);
        if(name.empty())
        {
            ec = error::bad_field;
            return;
        }
        if(*it++ != ':')
        {
            ec = error::bad_field;
            return;
        }
        if(*term != ' ' &&
           *term != '\t')
        {
            auto it2 = term - 2;
            detail::skip_ows(it, it2);
            detail::skip_ows_rev(it2, it);
            auto const value =
                make_string(it, it2);
            do_field(name, value, ec);
            if(ec)
                return;
            impl().on_field(name, value, ec);
            if(ec)
                return;
            it = term;
        }
        else
        {
            // obs-fold
            for(;;)
            {
                auto const it2 = term - 2;
                detail::skip_ows(it, it2);
                if(it != it2)
                    break;
                it = term;
                if(*it != ' ' && *it != '\t')
                    break;
                term = find_eol(it, last, ec);
                if(ec)
                    return;
            }
            std::string s;
            if(it != term)
            {
                s.append(it, term - 2);
                it = term;
                for(;;)
                {
                    if(*it != ' ' && *it != '\t')
                        break;
                    s.push_back(' ');
                    detail::skip_ows(it, term - 2);
                    term = find_eol(it, last, ec);
                    if(ec)
                        return;
                    if(it != term - 2)
                        s.append(it, term - 2);
                    it = term;
                }
            }
            string_view value{
                s.data(), s.size()};
            do_field(name, value, ec);
            if(ec)
                return;
            impl().on_field(name, value, ec);
            if(ec)
                return;
        }
    }
}

template<bool isRequest, bool isDirect, class Derived>
void
basic_parser<isRequest, isDirect, Derived>::
do_field(
    string_view const& name,
        string_view const& value,
            error_code& ec)
{
    // Connection
    if(strieq("connection", name) ||
        strieq("proxy-connection", name))
    {
        auto const list = opt_token_list{value};
        if(! validate_list(list))
        {
            // VFALCO Should this be a field specific error?
            ec = error::bad_value;
            return;
        }
        for(auto const& s : list)
        {
            if(strieq("close", s))
            {
                f_ |= flagConnectionClose;
                continue;
            }

            if(strieq("keep-alive", s))
            {
                f_ |= flagConnectionKeepAlive;
                continue;
            }

            if(strieq("upgrade", s))
            {
                f_ |= flagConnectionUpgrade;
                continue;
            }
        }
        return;
    }

    for(auto it = value.begin();
        it != value.end(); ++it)
    {
        if(! is_text(*it))
        {
            ec = error::bad_value;
            return;
        }
    }

    // Content-Length
    if(strieq("content-length", name))
    {
        if(f_ & flagContentLength)
        {
            // duplicate
            ec = error::bad_content_length;
            return;
        }

        if(f_ & flagChunked)
        {
            // conflicting field
            ec = error::bad_content_length;
            return;
        }

        std::uint64_t v;
        if(! parse_dec(
            value.begin(), value.end(), v))
        {
            ec = error::bad_content_length;
            return;
        }

        len_ = v;
        f_ |= flagContentLength;
        return;
    }

    // Transfer-Encoding
    if(strieq("transfer-encoding", name))
    {
        if(f_ & flagChunked)
        {
            // duplicate
            ec = error::bad_transfer_encoding;
            return;
        }

        if(f_ & flagContentLength)
        {
            // conflicting field
            ec = error::bad_transfer_encoding;
            return;
        }

        auto const v = token_list{value};
        auto const it = std::find_if(v.begin(), v.end(),
            [&](typename token_list::value_type const& s)
            {
                return strieq("chunked", s);
            });
        if(it == v.end())
            return;
        if(std::next(it) != v.end())
            return;
        len_ = 0;
        f_ |= flagChunked;
        return;
    }

    // Upgrade
    if(strieq("upgrade", name))
    {
        f_ |= flagUpgrade;
        ec = {};
        return;
    }
}

template<bool isRequest, bool isDirect, class Derived>
inline
std::size_t
basic_parser<isRequest, isDirect, Derived>::
parse_header(char const* p,
    std::size_t n, error_code& ec)
{
    if(n < 4)
        return 0;
    auto const term = find_eom(
        p + skip_, p + n, ec);
    if(ec)
        return 0;
    if(! term)
    {
        skip_ = n - 3;
        return 0;
    }

    int version;
    int status; // ignored for requests

    skip_ = 0;
    n = term - p;
    parse_startline(p, version, status, ec,
        std::integral_constant<
            bool, isRequest>{});
    if(ec)
        return 0;
    if(version >= 11)
        f_ |= flagHTTP11;

    parse_fields(p, term, ec);
    if(ec)
        return 0;
    BOOST_ASSERT(p == term);

    do_header(status,
        std::integral_constant<
            bool, isRequest>{});
    impl().on_header(ec);
    if(ec)
        return 0;
    if(state_ == parse_state::complete)
    {
        impl().on_complete(ec);
        if(ec)
            return 0;
    }
    return n;
}

template<bool isRequest, bool isDirect, class Derived>
void
basic_parser<isRequest, isDirect, Derived>::
do_header(int, std::true_type)
{
    // RFC 7230 section 3.3
    // https://tools.ietf.org/html/rfc7230#section-3.3

    if(f_ & flagSkipBody)
    {
        state_ = parse_state::complete;
    }
    else if(f_ & flagContentLength)
    {
        if(len_ > 0)
        {
            f_ |= flagHasBody;
            state_ = parse_state::body;
        }
        else
        {
            state_ = parse_state::complete;
        }
    }
    else if(f_ & flagChunked)
    {
        f_ |= flagHasBody;
        state_ = parse_state::chunk_header;
    }
    else
    {
        len_ = 0;
        state_ = parse_state::complete;
    }
}

template<bool isRequest, bool isDirect, class Derived>
void
basic_parser<isRequest, isDirect, Derived>::
do_header(int status, std::false_type)
{
    // RFC 7230 section 3.3
    // https://tools.ietf.org/html/rfc7230#section-3.3

    if( (f_ & flagSkipBody) ||  // e.g. response to a HEAD request
        status  / 100 == 1 ||   // 1xx e.g. Continue
        status == 204 ||        // No Content
        status == 304)          // Not Modified
    {
        state_ = parse_state::complete;
        return;
    }

    if(f_ & flagContentLength)
    {
        if(len_ > 0)
        {
            f_ |= flagHasBody;
            state_ = parse_state::body;
        }
        else
        {
            state_ = parse_state::complete;
        }
    }
    else if(f_ & flagChunked)
    {
        f_ |= flagHasBody;
        state_ = parse_state::chunk_header;
    }
    else
    {
        f_ |= flagHasBody;
        f_ |= flagNeedEOF;
        state_ = parse_state::body_to_eof;
    }
}

template<bool isRequest, bool isDirect, class Derived>
void
basic_parser<isRequest, isDirect, Derived>::
maybe_do_body_direct()
{
    if(f_ & flagOnBody)
        return;
    f_ |= flagOnBody;
    if(got_content_length())
        impl().on_body(len_);
    else
        impl().on_body();
}

template<bool isRequest, bool isDirect, class Derived>
void
basic_parser<isRequest, isDirect, Derived>::
maybe_do_body_indirect(error_code& ec)
{
    if(f_ & flagOnBody)
        return;
    f_ |= flagOnBody;
    if(got_content_length())
    {
        impl().on_body(len_, ec);
        if(ec)
            return;
    }
    else
    {
        impl().on_body(ec);
        if(ec)
            return;
    }
}

template<bool isRequest, bool isDirect, class Derived>
std::size_t
basic_parser<isRequest, isDirect, Derived>::
parse_chunk_header(char const* p,
    std::size_t n, error_code& ec)
{
/*
    chunked-body   = *chunk last-chunk trailer-part CRLF

    chunk          = chunk-size [ chunk-ext ] CRLF chunk-data CRLF
    last-chunk     = 1*("0") [ chunk-ext ] CRLF
    trailer-part   = *( header-field CRLF )

    chunk-size     = 1*HEXDIG
    chunk-data     = 1*OCTET ; a sequence of chunk-size octets
    chunk-ext      = *( ";" chunk-ext-name [ "=" chunk-ext-val ] )
    chunk-ext-name = token
    chunk-ext-val  = token / quoted-string
*/

    auto const first = p;
    auto const last = p + n;

    // Treat the last CRLF in a chunk as
    // part of the next chunk, so it can
    // be parsed in one call instead of two.
    if(f_ & flagExpectCRLF)
    {
        if(n < 2)
            return 0;
        if(! parse_crlf(p))
        {
            ec = error::bad_chunk;
            return 0;
        }
        n -= 2;
    }

    char const* term;

    if(! (f_ & flagFinalChunk))
    {
        if(n < 2)
            return 0;
        term = find_eol(p + skip_, last, ec);
        if(ec)
            return 0;
        if(! term)
        {
            skip_ = n - 1;
            return 0;
        }
        std::uint64_t v;
        if(! parse_hex(p, v))
        {
            ec = error::bad_chunk;
            return 0;
        }
        if(v != 0)
        {
            if(*p == ';')
            {
                // VFALCO We need to parse the chunk
                // extension to validate it here.
                ext_ = make_string(p, term - 2);
                impl().on_chunk(v, ext_, ec);
                if(ec)
                    return 0;
            }
            else if(p != term - 2)
            {
                ec = error::bad_chunk;
                return 0;
            }
            p = term;
            len_ = v;
            skip_ = 0;
            f_ |= flagExpectCRLF;
            state_ = parse_state::chunk_body;
            return p - first;
        }

        // This is the offset from the buffer
        // to the beginning of the first '\r\n'
        x_ = term - 2 - first;
        skip_ = x_;

        f_ |= flagFinalChunk;
    }
    else
    {
        // We are parsing the value again
        // to advance p to the right place.
        std::uint64_t v;
        auto const result = parse_hex(p, v);
        BOOST_ASSERT(result && v == 0);
        beast::detail::ignore_unused(result);
        beast::detail::ignore_unused(v);
    }

    term = find_eom(
        first + skip_, last, ec);
    if(ec)
        return 0;
    if(! term)
    {
        if(n > 3)
            skip_ = (last - first) - 3;
        return 0;
    }

    if(*p == ';')
    {
        ext_ = make_string(p, first + x_);
        impl().on_chunk(0, ext_, ec);
        if(ec)
            return 0;
        p = first + x_;
    }
    if(! parse_crlf(p))
    {
        ec = error::bad_chunk;
        return 0;
    }
    parse_fields(p, term, ec);
    if(ec)
        return 0;
    BOOST_ASSERT(p == term);

    do_complete(ec);
    if(ec)
        return 0;
    return p - first;
}

template<bool isRequest, bool isDirect, class Derived>
inline
std::size_t
basic_parser<isRequest, isDirect, Derived>::
parse_body(char const* p,
    std::size_t n, error_code& ec)
{
    n = beast::detail::clamp(len_, n);
    body_ = string_view{p, n};
    impl().on_data(body_, ec);
    if(ec)
        return 0;
    len_ -= n;
    if(len_ == 0)
    {
        do_complete(ec);
        if(ec)
            return 0;
    }
    return n;
}

template<bool isRequest, bool isDirect, class Derived>
inline
std::size_t
basic_parser<isRequest, isDirect, Derived>::
parse_body_to_eof(char const* p,
    std::size_t n, error_code& ec)
{
    body_ = string_view{p, n};
    impl().on_data(body_, ec);
    if(ec)
        return 0;
    return n;
}

template<bool isRequest, bool isDirect, class Derived>
inline
std::size_t
basic_parser<isRequest, isDirect, Derived>::
parse_chunk_body(char const* p,
    std::size_t n, error_code& ec)
{
    n = beast::detail::clamp(len_, n);
    body_ = string_view{p, n};
    impl().on_data(body_, ec);
    if(ec)
        return 0;
    len_ -= n;
    if(len_ == 0)
    {
        body_ = {};
        state_ = parse_state::chunk_header;
    }
    return n;
}

template<bool isRequest, bool isDirect, class Derived>
void
basic_parser<isRequest, isDirect, Derived>::
do_complete(error_code& ec)
{
    impl().on_complete(ec);
    if(ec)
        return;
    state_ = parse_state::complete;
}

} // http
} // beast

#endif
