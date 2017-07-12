//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_CHUNK_ENCODE_IPP
#define BEAST_HTTP_IMPL_CHUNK_ENCODE_IPP

#include <beast/http/detail/rfc7230.hpp>

namespace beast {
namespace http {

inline
chunk_header::
chunk_header(std::size_t size)
    : view_(
        size,
        boost::asio::const_buffers_1{nullptr, 0},
        chunk_crlf{})
{
    BOOST_ASSERT(size > 0);
}

inline
chunk_header::
chunk_header(
    std::size_t size,
    string_view extensions)
    : view_(
        size,
        boost::asio::const_buffers_1{
            extensions.data(), extensions.size()},
        chunk_crlf{})
{
    BOOST_ASSERT(size > 0);
}

template<class ChunkExtensions, class>
chunk_header::
chunk_header(
    std::size_t size,
    ChunkExtensions&& extensions)
    : exts_(std::make_shared<detail::chunk_extensions_impl<
        typename std::decay<ChunkExtensions>::type>>(
            std::forward<ChunkExtensions>(extensions)))
    , view_(
        size,
        exts_->str(),
        chunk_crlf{})
{
    static_assert(
        detail::is_chunk_extensions<ChunkExtensions>::value,
        "ChunkExtensions requirements not met");
    BOOST_ASSERT(size > 0);
}

template<class ChunkExtensions, class Allocator, class>
chunk_header::
chunk_header(
    std::size_t size,
    ChunkExtensions&& extensions,
    Allocator const& allocator)
    : exts_(std::allocate_shared<detail::chunk_extensions_impl<
        typename std::decay<ChunkExtensions>::type>>(allocator,
            std::forward<ChunkExtensions>(extensions)))
    , view_(
        size,
        exts_->str(),
        chunk_crlf{})
{
    static_assert(
        detail::is_chunk_extensions<ChunkExtensions>::value,
        "ChunkExtensions requirements not met");
    BOOST_ASSERT(size > 0);
}

//------------------------------------------------------------------------------

template<class ConstBufferSequence>
chunk_body<ConstBufferSequence>::
chunk_body(ConstBufferSequence const& buffers)
    : view_(
        boost::asio::buffer_size(buffers),
        boost::asio::const_buffers_1{nullptr, 0},
        chunk_crlf{},
        buffers,
        chunk_crlf{})
{
}

template<class ConstBufferSequence>
chunk_body<ConstBufferSequence>::
chunk_body(
    ConstBufferSequence const& buffers,
    string_view extensions)
    : view_(
        boost::asio::buffer_size(buffers),
        boost::asio::const_buffers_1{
            extensions.data(), extensions.size()},
        chunk_crlf{},
        buffers,
        chunk_crlf{})
{
}

template<class ConstBufferSequence>
template<class ChunkExtensions, class>
chunk_body<ConstBufferSequence>::
chunk_body(
    ConstBufferSequence const& buffers,
    ChunkExtensions&& extensions)
    : exts_(std::make_shared<detail::chunk_extensions_impl<
        typename std::decay<ChunkExtensions>::type>>(
            std::forward<ChunkExtensions>(extensions)))
    , view_(
        boost::asio::buffer_size(buffers),
        exts_->str(),
        chunk_crlf{},
        buffers,
        chunk_crlf{})
{
}

template<class ConstBufferSequence>
template<class ChunkExtensions, class Allocator, class>
chunk_body<ConstBufferSequence>::
chunk_body(
    ConstBufferSequence const& buffers,
    ChunkExtensions&& extensions,
    Allocator const& allocator)
    : exts_(std::allocate_shared<detail::chunk_extensions_impl<
        typename std::decay<ChunkExtensions>::type>>(allocator,
            std::forward<ChunkExtensions>(extensions)))
    , view_(
        boost::asio::buffer_size(buffers),
        exts_->str(),
        chunk_crlf{},
        buffers,
        chunk_crlf{})
{
}

//------------------------------------------------------------------------------

template<class Trailer>
template<class Allocator>
auto
chunk_last<Trailer>::
prepare(Trailer const& trailer, Allocator const& allocator) ->
    buffers_type
{
    auto sp = std::allocate_shared<typename
        Trailer::reader>(allocator, trailer);
    sp_ = sp;
    return sp->get();
}

template<class Trailer>
auto
chunk_last<Trailer>::
prepare(Trailer const& trailer, std::true_type) ->
    buffers_type
{
    auto sp = std::make_shared<
        typename Trailer::reader>(trailer);
    sp_ = sp;
    return sp->get();
}

template<class Trailer>
auto
chunk_last<Trailer>::
prepare(Trailer const& trailer, std::false_type) ->
    buffers_type
{
    return trailer;
}

template<class Trailer>
chunk_last<Trailer>::
chunk_last()
    : view_(
        detail::chunk_size0{},
        Trailer{})
{
}

template<class Trailer>
chunk_last<Trailer>::
chunk_last(Trailer const& trailer)
    : view_(
        detail::chunk_size0{},
        prepare(trailer, is_fields<Trailer>{}))
{
}

template<class Trailer>
template<class DeducedTrailer, class Allocator, class>
chunk_last<Trailer>::
chunk_last(
    DeducedTrailer const& trailer, Allocator const& allocator)
    : view_(
        detail::chunk_size0{},
        prepare(trailer, allocator))
{
}

//------------------------------------------------------------------------------

template<class Allocator>
void
basic_chunk_extensions<Allocator>::
insert(string_view name)
{
/*
    chunk-ext       = *( ";" chunk-ext-name [ "=" chunk-ext-val ] )
*/
    s_.reserve(1 + name.size());
    s_.push_back(';');
    s_.append(name.data(), name.size());
}

template<class Allocator>
void
basic_chunk_extensions<Allocator>::
insert(string_view name, string_view value)
{
/*
    chunk-ext       = *( ";" chunk-ext-name [ "=" chunk-ext-val ] )
    chunk-ext-name  = token
    chunk-ext-val   = token / quoted-string
    token           = 1*tchar
    quoted-string   = DQUOTE *( qdtext / quoted-pair ) DQUOTE
    qdtext          = HTAB / SP / "!" / %x23-5B ; '#'-'[' / %x5D-7E ; ']'-'~' / obs-text
    quoted-pair     = "\" ( HTAB / SP / VCHAR / obs-text )
    obs-text        = %x80-FF
*/
    bool is_token = true;
    for(auto const c : value)
    {
        if(! detail::is_tchar(c))
        {
            is_token = false;
            break;
        }
    }
    if(is_token)
    {
        s_.reserve(1 + name.size() + 1 + value.size());
        s_.push_back(';');
        s_.append(name.data(), name.size());
        s_.push_back('=');
        s_.append(value.data(), value.size());
        return;
    }

    // quoted-string

    s_.reserve(
        1 + name.size() + 1 +
        1 + value.size() + 20 + 1);
    s_.push_back(';');
    s_.append(name.data(), name.size());
    s_.append("=\"", 2);
    for(auto const c : value)
    {
        if(c == '\\')
            s_.append(R"(\\)", 2);
        else if(c == '\"')
            s_.append(R"(\")", 2);
        else
            s_.push_back(c);
    }
    s_.push_back('"');
}

} // http
} // beast

#endif
