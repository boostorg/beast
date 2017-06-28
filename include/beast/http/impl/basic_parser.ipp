//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_BASIC_PARSER_IPP
#define BEAST_HTTP_IMPL_BASIC_PARSER_IPP

#include <beast/core/static_string.hpp>
#include <beast/core/type_traits.hpp>
#include <beast/core/detail/clamp.hpp>
#include <beast/core/detail/config.hpp>
#include <beast/http/error.hpp>
#include <beast/http/rfc7230.hpp>
#include <boost/asio/buffer.hpp>
#include <algorithm>
#include <utility>

namespace beast {
namespace http {

namespace detail {

template<class FwdIt>
inline
FwdIt
skip_ows2(FwdIt it, FwdIt const& end)
{
    while(it != end)
    {
        if(*it != ' ' && *it != '\t')
            break;
        ++it;
    }
    return it;
}

template<class RanIt>
inline
RanIt
skip_ows_rev2(
    RanIt it, RanIt const& first)
{
    while(it != first)
    {
        auto const c = it[-1];
        if(c != ' ' && c != '\t')
            break;
        --it;
    }
    return it;
}

} // detail

template<bool isRequest, class Derived>
basic_parser<isRequest, Derived>::
basic_parser()
    : body_limit_(
        default_body_limit(is_request{}))
{
}

template<bool isRequest, class Derived>
template<class OtherDerived>
basic_parser<isRequest, Derived>::
basic_parser(basic_parser<
        isRequest, OtherDerived>&& other)
    : body_limit_(other.body_limit_)
    , len_(other.len_)
    , buf_(std::move(other.buf_))
    , buf_len_(other.buf_len_)
    , skip_(other.skip_)
    , state_(other.state_)
    , f_(other.f_)
{
}

template<bool isRequest, class Derived>
bool
basic_parser<isRequest, Derived>::
is_keep_alive() const
{
    BOOST_ASSERT(is_header_done());
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

template<bool isRequest, class Derived>
boost::optional<std::uint64_t>
basic_parser<isRequest, Derived>::
content_length() const
{
    BOOST_ASSERT(is_header_done());
    if(! (f_ & flagContentLength))
        return boost::none;
    return len_;
}

template<bool isRequest, class Derived>
void
basic_parser<isRequest, Derived>::
skip(bool v)
{
    BOOST_ASSERT(! got_some());
    if(v)
        f_ |= flagSkipBody;
    else
        f_ &= ~flagSkipBody;
}

template<bool isRequest, class Derived>
template<class ConstBufferSequence>
std::size_t
basic_parser<isRequest, Derived>::
put(ConstBufferSequence const& buffers,
    error_code& ec)
{
    static_assert(is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    using boost::asio::buffer_cast;
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    auto const p = buffers.begin();
    auto const last = buffers.end();
    if(p == last)
    {
        ec.assign(0, ec.category());
        return 0;
    }
    if(std::next(p) == last)
    {
        // single buffer
        auto const b = *p;
        return put(boost::asio::const_buffers_1{
            buffer_cast<char const*>(b),
            buffer_size(b)}, ec);
    }
    auto const size = buffer_size(buffers);
    if(size <= max_stack_buffer)
        return put_from_stack(size, buffers, ec);
    if(size > buf_len_)
    {
        // reallocate
        buf_.reset(new char[size]);
        buf_len_ = size;
    }
    // flatten
    buffer_copy(boost::asio::buffer(
        buf_.get(), buf_len_), buffers);
    return put(boost::asio::const_buffers_1{
        buf_.get(), buf_len_}, ec);
}

template<bool isRequest, class Derived>
std::size_t
basic_parser<isRequest, Derived>::
put(boost::asio::const_buffers_1 const& buffer,
    error_code& ec)
{
    BOOST_ASSERT(state_ != state::complete);
    using boost::asio::buffer_size;
    auto p = boost::asio::buffer_cast<
        char const*>(*buffer.begin());
    auto n = buffer_size(*buffer.begin());
    auto const p0 = p;
    auto const p1 = p0 + n;
loop:
    switch(state_)
    {
    case state::nothing_yet:
        if(n == 0)
        {
            ec = error::need_more;
            return 0;
        }
        state_ = state::header;
        BEAST_FALLTHROUGH;

    case state::header:
        parse_header(p, n, ec);
        if(ec)
            goto done;
        break;

    case state::body0:
        impl().on_body(content_length(), ec);
        if(ec)
            goto done;
        state_ = state::body;
        BEAST_FALLTHROUGH;

    case state::body:
        parse_body(p, n, ec);
        if(ec)
            goto done;
        break;

    case state::body_to_eof0:
        impl().on_body(content_length(), ec);
        if(ec)
            goto done;
        state_ = state::body_to_eof;
        BEAST_FALLTHROUGH;

    case state::body_to_eof:
        parse_body_to_eof(p, n, ec);
        if(ec)
            goto done;
        break;

    case state::chunk_header0:
        impl().on_body(content_length(), ec);
        if(ec)
            goto done;
        state_ = state::chunk_header;
        BEAST_FALLTHROUGH;

    case state::chunk_header:
        parse_chunk_header(p, n, ec);
        if(ec)
            goto done;
        break;

    case state::chunk_body:
        parse_chunk_body(p, n, ec);
        if(ec)
            goto done;
        break;

    case state::complete:
        ec.assign(0, ec.category());
        goto done;
    }
    if(p < p1 && ! is_done() && eager())
    {
        n = static_cast<std::size_t>(p1 - p);
        goto loop;
    }
done:
    return static_cast<std::size_t>(p - p0);
}

template<bool isRequest, class Derived>
void
basic_parser<isRequest, Derived>::
put_eof(error_code& ec)
{
    BOOST_ASSERT(got_some());
    if(state_ == state::header)
    {
        ec = error::partial_message;
        return;
    }
    if(f_ & (flagContentLength | flagChunked))
    {
        if(state_ != state::complete)
        {
            ec = error::partial_message;
            return;
        }
        ec.assign(0, ec.category());
        return;
    }
    impl().on_complete(ec);
    if(ec)
        return;
    state_ = state::complete;
}

template<bool isRequest, class Derived>
template<class ConstBufferSequence>
std::size_t
basic_parser<isRequest, Derived>::
put_from_stack(std::size_t size,
    ConstBufferSequence const& buffers,
        error_code& ec)
{
    char buf[max_stack_buffer];
    using boost::asio::buffer;
    using boost::asio::buffer_copy;
    buffer_copy(buffer(buf, sizeof(buf)), buffers);
    return put(boost::asio::const_buffers_1{
        buf, size}, ec);
}

template<bool isRequest, class Derived>
inline
void
basic_parser<isRequest, Derived>::
parse_header(char const*& p,
    std::size_t n, error_code& ec)
{
    if( n > header_limit_)
        n = header_limit_;
    if(n < skip_ + 4)
    {
        ec = error::need_more;
        return;
    }
    auto const term =
        find_eom(p + skip_, p + n);
    if(! term)
    {
        skip_ = n - 3;
        if(skip_ + 4 > header_limit_)
        {
            ec = error::header_limit;
            return;
        }
        ec = http::error::need_more;
        return;
    }
    skip_ = 0;

    parse_header(p, term, ec,
        std::integral_constant<bool, isRequest>{});
    if(ec)
        return;

    impl().on_header(ec);
    if(ec)
        return;
    if(state_ == state::complete)
    {
        impl().on_complete(ec);
        if(ec)
            return;
    }
}

template<bool isRequest, class Derived>
void
basic_parser<isRequest, Derived>::
parse_header(char const*& p, char const* term,
    error_code& ec, std::true_type)
{
/*
    request-line   = method SP request-target SP HTTP-version CRLF
    method         = token
*/
    auto const method = parse_method(p);
    if(method.empty())
    {
        ec = error::bad_method;
        return;
    }
    if(*p++ != ' ')
    {
        ec = error::bad_method;
        return;
    }

    auto const target = parse_target(p);
    if(target.empty())
    {
        ec = error::bad_target;
        return;
    }
    if(*p++ != ' ')
    {
        ec = error::bad_target;
        return;
    }

    auto const version = parse_version(p);
    if(version < 0)
    {
        ec = error::bad_version;
        return;
    }

    if(! parse_crlf(p))
    {
        ec = error::bad_version;
        return;
    }

    if(version >= 11)
        f_ |= flagHTTP11;

    impl().on_request(string_to_verb(method),
        method, target, version, ec);
    if(ec)
        return;

    parse_fields(p, term, ec);
    if(ec)
        return;
    BOOST_ASSERT(p == term);

    // RFC 7230 section 3.3
    // https://tools.ietf.org/html/rfc7230#section-3.3

    if(f_ & flagSkipBody)
    {
        state_ = state::complete;
    }
    else if(f_ & flagContentLength)
    {
        if(len_ > 0)
        {
            f_ |= flagHasBody;
            state_ = state::body0;
        }
        else
        {
            state_ = state::complete;
        }
    }
    else if(f_ & flagChunked)
    {
        f_ |= flagHasBody;
        state_ = state::chunk_header0;
    }
    else
    {
        len_ = 0;
        state_ = state::complete;
    }
}

template<bool isRequest, class Derived>
void
basic_parser<isRequest, Derived>::
parse_header(char const*& p, char const* term,
    error_code& ec, std::false_type)
{
/*
     status-line    = HTTP-version SP status-code SP reason-phrase CRLF
     status-code    = 3*DIGIT
     reason-phrase  = *( HTAB / SP / VCHAR / obs-text )
*/
    auto const version = parse_version(p);
    if(version < 0 || *p != ' ')
    {
        ec = error::bad_version;
        return;
    }
    ++p;

    auto const status = parse_status(p);
    if(status < 0 || *p != ' ')
    {
        ec = error::bad_status;
        return;
    }
    ++p;

    auto const reason = parse_reason(p);
    if(! parse_crlf(p))
    {
        ec = error::bad_reason;
        return;
    }

    if(version >= 11)
        f_ |= flagHTTP11;

    impl().on_response(
        status, reason, version, ec);
    if(ec)
        return;

    parse_fields(p, term, ec);
    if(ec)
        return;
    BOOST_ASSERT(p == term);

    // RFC 7230 section 3.3
    // https://tools.ietf.org/html/rfc7230#section-3.3

    if( (f_ & flagSkipBody) ||  // e.g. response to a HEAD request
        status  / 100 == 1 ||   // 1xx e.g. Continue
        status == 204 ||        // No Content
        status == 304)          // Not Modified
    {
        state_ = state::complete;
        return;
    }

    if(f_ & flagContentLength)
    {
        if(len_ > 0)
        {
            f_ |= flagHasBody;
            state_ = state::body0;
        }
        else
        {
            state_ = state::complete;
        }
    }
    else if(f_ & flagChunked)
    {
        f_ |= flagHasBody;
        state_ = state::chunk_header0;
    }
    else
    {
        f_ |= flagHasBody;
        f_ |= flagNeedEOF;
        state_ = state::body_to_eof0;
    }
}

template<bool isRequest, class Derived>
inline
void
basic_parser<isRequest, Derived>::
parse_body(char const*& p,
    std::size_t n, error_code& ec)
{
    n = beast::detail::clamp(len_, n);
    impl().on_data(string_view{p, n}, ec);
    if(ec)
        return;
    p += n;
    len_ -= n;
    if(len_ > 0)
        return;
    impl().on_complete(ec);
    if(ec)
        return;
    state_ = state::complete;
}

template<bool isRequest, class Derived>
inline
void
basic_parser<isRequest, Derived>::
parse_body_to_eof(char const*& p,
    std::size_t n, error_code& ec)
{
    if(n > body_limit_)
    {
        ec = error::body_limit;
        return;
    }
    body_limit_ = body_limit_ - n;
    impl().on_data(string_view{p, n}, ec);
    if(ec)
        return;
    p += n;
}

template<bool isRequest, class Derived>
void
basic_parser<isRequest, Derived>::
parse_chunk_header(char const*& p0,
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

    auto p = p0;
    auto const pend = p + n;
    char const* eol;

    if(! (f_ & flagFinalChunk))
    {
        if(n < skip_ + 2)
        {
            ec = error::need_more;
            return;
        }
        if(f_ & flagExpectCRLF)
        {
            // Treat the last CRLF in a chunk as
            // part of the next chunk, so p can
            // be parsed in one call instead of two.
            if(! parse_crlf(p))
            {
                ec = error::bad_chunk;
                return;
            }
        }
        eol = find_eol(p0 + skip_, pend, ec);
        if(ec)
            return;
        if(! eol)
        {
            ec = error::need_more;
            skip_ = n - 1;
            return;
        }
        skip_ = static_cast<
            std::size_t>(eol - 2 - p0);

        std::uint64_t v;
        if(! parse_hex(p, v))
        {
            ec = error::bad_chunk;
            return;
        }
        if(v != 0)
        {
            if(v > body_limit_)
            {
                ec = error::body_limit;
                return;
            }
            body_limit_ -= v;
            if(*p == ';')
            {
                // VFALCO TODO Validate extension
                impl().on_chunk(v, make_string(
                    p, eol - 2), ec);
                if(ec)
                    return;
            }
            else if(p != eol - 2)
            {
                ec = error::bad_chunk;
                return;
            }
            impl().on_chunk(v, {}, ec);
            if(ec)
                return;
            len_ = v;
            skip_ = 2;
            p0 = eol;
            f_ |= flagExpectCRLF;
            state_ = state::chunk_body;
            return;
        }

        f_ |= flagFinalChunk;
    }
    else
    {
        BOOST_ASSERT(n >= 5);
        if(f_ & flagExpectCRLF)
            BOOST_VERIFY(parse_crlf(p));
        std::uint64_t v;
        BOOST_VERIFY(parse_hex(p, v));
        eol = find_eol(p, pend, ec);
        BOOST_ASSERT(! ec);
    }

    auto eom = find_eom(p0 + skip_, pend);
    if(! eom)
    {
        BOOST_ASSERT(n >= 3);
        skip_ = n - 3;
        ec = error::need_more;
        return;
    }

    if(*p == ';')
    {
        // VFALCO TODO Validate extension
        impl().on_chunk(0, make_string(
            p, eol - 2), ec);
        if(ec)
            return;
    }
    p = eol;
    parse_fields(p, eom, ec);
    if(ec)
        return;
    BOOST_ASSERT(p == eom);
    p0 = eom;

    impl().on_complete(ec);
    if(ec)
        return;
    state_ = state::complete;
}

template<bool isRequest, class Derived>
inline
void
basic_parser<isRequest, Derived>::
parse_chunk_body(char const*& p,
    std::size_t n, error_code& ec)
{
    n = beast::detail::clamp(len_, n);
    impl().on_data(string_view{p, n}, ec);
    if(ec)
        return;
    p += n;
    len_ -= n;
    if(len_ > 0)
        return;
    state_ = state::chunk_header;
}

template<bool isRequest, class Derived>
void
basic_parser<isRequest, Derived>::
parse_fields(char const*& p,
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
        auto term = find_eol(p, last, ec);
        if(ec)
            return;
        BOOST_ASSERT(term);
        if(p == term - 2)
        {
            p = term;
            break;
        }
        auto const name = parse_name(p);
        if(name.empty())
        {
            ec = error::bad_field;
            return;
        }
        if(*p++ != ':')
        {
            ec = error::bad_field;
            return;
        }
        if(*term != ' ' &&
           *term != '\t')
        {
            auto it2 = term - 2;
            p = detail::skip_ows2(p, it2);
            it2 = detail::skip_ows_rev2(it2, p);
            auto const f = string_to_field(name);
            auto const value = make_string(p, it2);
            do_field(f, value, ec);
            if(ec)
                return;
            impl().on_field(f, name, value, ec);
            if(ec)
                return;
            p = term;
        }
        else
        {
            // obs-fold
            for(;;)
            {
                auto const it2 = term - 2;
                p = detail::skip_ows2(p, it2);
                if(p != it2)
                    break;
                p = term;
                if(*p != ' ' && *p != '\t')
                    break;
                term = find_eol(p, last, ec);
                if(ec)
                    return;
            }
            // https://stackoverflow.com/questions/686217/maximum-on-http-header-values
            static_string<max_obs_fold> s;
            try
            {
                if(p != term)
                {
                    s.append(p, term - 2);
                    p = term;
                    for(;;)
                    {
                        if(*p != ' ' && *p != '\t')
                            break;
                        s.push_back(' ');
                        p = detail::skip_ows2(p, term - 2);
                        term = find_eol(p, last, ec);
                        if(ec)
                            return;
                        if(p != term - 2)
                            s.append(p, term - 2);
                        p = term;
                    }
                }
            }
            catch(std::length_error const&)
            {
                ec = error::bad_obs_fold;
                return;
            }
            auto const f = string_to_field(name);
            string_view const value{s.data(), s.size()};
            do_field(f, value, ec);
            if(ec)
                return;
            impl().on_field(f, name, value, ec);
            if(ec)
                return;
        }
    }
}

template<bool isRequest, class Derived>
void
basic_parser<isRequest, Derived>::
do_field(field f,
    string_view value, error_code& ec)
{
    // Connection
    if(f == field::connection ||
        f == field::proxy_connection)
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
        ec.assign(0, ec.category());
        return;
    }

    for(auto p = value.begin();
        p != value.end(); ++p)
    {
        if(! is_text(*p))
        {
            ec = error::bad_value;
            return;
        }
    }

    // Content-Length
    if(f == field::content_length)
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

        if(v > body_limit_)
        {
            ec = error::body_limit;
            return;
        }

        ec.assign(0, ec.category());
        len_ = v;
        f_ |= flagContentLength;
        return;
    }

    // Transfer-Encoding
    if(f == field::transfer_encoding)
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

        ec.assign(0, ec.category());
        auto const v = token_list{value};
        auto const p = std::find_if(v.begin(), v.end(),
            [&](typename token_list::value_type const& s)
            {
                return strieq("chunked", s);
            });
        if(p == v.end())
            return;
        if(std::next(p) != v.end())
            return;
        len_ = 0;
        f_ |= flagChunked;
        return;
    }

    // Upgrade
    if(f == field::upgrade)
    {
        ec.assign(0, ec.category());
        f_ |= flagUpgrade;
        return;
    }

    ec.assign(0, ec.category());
}

} // http
} // beast

#endif
