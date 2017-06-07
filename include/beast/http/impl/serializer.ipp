//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_SERIALIZER_IPP
#define BEAST_HTTP_IMPL_SERIALIZER_IPP

#include <beast/http/error.hpp>
#include <beast/http/status.hpp>
#include <boost/assert.hpp>
#include <ostream>

namespace beast {
namespace http {

template<bool isRequest, class Body, class Fields,
    class ChunkDecorator, class Allocator>
void
serializer<isRequest, Body, Fields,
    ChunkDecorator, Allocator>::
frdinit(std::true_type)
{
    frd_.emplace(m_, m_.version, m_.method());
}

template<bool isRequest, class Body, class Fields,
    class ChunkDecorator, class Allocator>
void
serializer<isRequest, Body, Fields,
    ChunkDecorator, Allocator>::
frdinit(std::false_type)
{
    frd_.emplace(m_, m_.version, m_.result_int());
}

template<bool isRequest, class Body, class Fields,
    class ChunkDecorator, class Allocator>
serializer<isRequest, Body, Fields,
    ChunkDecorator, Allocator>::
serializer(message<isRequest, Body, Fields> const& m,
        ChunkDecorator const& d, Allocator const& alloc)
    : m_(m)
    , d_(d)
{
}

template<bool isRequest, class Body, class Fields,
    class ChunkDecorator, class Allocator>
template<class Visit>
void
serializer<isRequest, Body, Fields,
    ChunkDecorator, Allocator>::
get(error_code& ec, Visit&& visit)
{
    using boost::asio::buffer_size;
    switch(s_)
    {
    case do_construct:
    {
        frdinit(std::integral_constant<bool,
            isRequest>{});
        close_ = m_.has_close() || (
            m_.version < 11 && ! m_.has_content_length());
        if(m_.has_chunked())
            goto go_init_c;
        s_ = do_init;
        // [[fallthrough]]
    }

    case do_init:
    {
        if(split_)
            goto go_header_only;
        rd_.emplace(m_);
        rd_->init(ec);
        if(ec)
            return;
        auto result = rd_->get(ec);
        if(ec)
        {
            // Can't use need_more when ! is_deferred
            BOOST_ASSERT(ec != error::need_more);
            return;
        }
        if(! result)
            goto go_header_only;
        more_ = result->second;
        v_ = cb0_t{
            boost::in_place_init,
            frd_->get(),
            result->first};
        s_ = do_header;
        // [[fallthrough]]
    }

    case do_header:
        visit(ec, boost::get<cb0_t>(v_));
        break;

    go_header_only:
        v_ = ch_t{frd_->get()};
        s_ = do_header_only;
    case do_header_only:
        visit(ec, boost::get<ch_t>(v_));
        break;

    case do_body:
        BOOST_ASSERT(! rd_);
        rd_.emplace(m_);
        rd_->init(ec);
        if(ec)
            return;
        s_ = do_body + 1;
        // [[fallthrough]]

    case do_body + 1:
    {
        auto result = rd_->get(ec);
        if(ec)
            return;
        if(! result)
            goto go_complete;
        more_ = result->second;
        v_ = cb1_t{result->first};
        s_ = do_body + 2;
        // [[fallthrough]]
    }

    case do_body + 2:
        visit(ec, boost::get<cb1_t>(v_));
        break;

    //----------------------------------------------------------------------

         go_init_c:
    s_ = do_init_c;
    case do_init_c:
    {
        if(split_)
            goto go_header_only_c;
        rd_.emplace(m_);
        rd_->init(ec);
        if(ec)
            return;
        auto result = rd_->get(ec);
        if(ec)
        {
            // Can't use need_more when ! is_deferred
            BOOST_ASSERT(ec != error::need_more);
            return;
        }
        if(! result)
            goto go_header_only_c;
        more_ = result->second;
        v_ = ch0_t{
            boost::in_place_init,
            frd_->get(),
            detail::chunk_header{
                buffer_size(result->first)},
            [&]()
            {
                auto sv = d_(result->first);
                return boost::asio::const_buffers_1{
                    sv.data(), sv.size()};
                
            }(),
            detail::chunk_crlf(),
            result->first,
            detail::chunk_crlf()};
        s_ = do_header_c;
        // [[fallthrough]]
    }

    case do_header_c:
        visit(ec, boost::get<ch0_t>(v_));
        break;

    go_header_only_c:
        v_ = ch_t{frd_->get()};
        s_ = do_header_only_c;
    case do_header_only_c:
        visit(ec, boost::get<ch_t>(v_));
        break;

    case do_body_c:
        BOOST_ASSERT(! rd_);
        rd_.emplace(m_);
        rd_->init(ec);
        if(ec)
            return;
        s_ = do_body_c + 1;
        // [[fallthrough]]

    case do_body_c + 1:
    {
        auto result = rd_->get(ec);
        if(ec)
            return;
        if(! result)
            goto go_final_c;
        more_ = result->second;
        v_ = ch1_t{
            boost::in_place_init,
            detail::chunk_header{
                buffer_size(result->first)},
            [&]()
            {
                auto sv = d_(result->first);
                return boost::asio::const_buffers_1{
                    sv.data(), sv.size()};
                
            }(),
            detail::chunk_crlf(),
            result->first,
            detail::chunk_crlf()};
        s_ = do_body_c + 2;
        // [[fallthrough]]
    }

    case do_body_c + 2:
        visit(ec, boost::get<ch1_t>(v_));
        break;

         go_final_c:
    case do_final_c:
        v_ = ch2_t{
            boost::in_place_init,
            detail::chunk_final(),
            [&]()
            {
                auto sv = d_(
                    boost::asio::null_buffers{});
                return boost::asio::const_buffers_1{
                    sv.data(), sv.size()};
                
            }(),
            detail::chunk_crlf()};
        s_ = do_final_c + 1;
        // [[fallthrough]]

    case do_final_c + 1:
        visit(ec, boost::get<ch2_t>(v_));
        break;

    //----------------------------------------------------------------------

    default:
    case do_complete:
        BOOST_ASSERT(false);
        break;

    go_complete:
        s_ = do_complete;
        break;
    }
}

template<bool isRequest, class Body, class Fields,
    class ChunkDecorator, class Allocator>
void
serializer<isRequest, Body, Fields,
    ChunkDecorator, Allocator>::
consume(std::size_t n)
{
    using boost::asio::buffer_size;
    switch(s_)
    {
    case do_header:
        BOOST_ASSERT(n <= buffer_size(
            boost::get<cb0_t>(v_)));
        boost::get<cb0_t>(v_).consume(n);
        if(buffer_size(boost::get<cb0_t>(v_)) > 0)
            break;
        header_done_ = true;
        v_ = boost::blank{};
        if(! more_)
            goto go_complete;
        s_ = do_body + 1;
        break;

    case do_header_only:
        BOOST_ASSERT(n <= buffer_size(
            boost::get<ch_t>(v_)));
        boost::get<ch_t>(v_).consume(n);
        if(buffer_size(boost::get<ch_t>(v_)) > 0)
            break;
        frd_ = boost::none;
        header_done_ = true;
        if(! split_)
            goto go_complete;
        s_ = do_body;
        break;

    case do_body + 2:
    {
        BOOST_ASSERT(n <= buffer_size(
            boost::get<cb1_t>(v_)));
        boost::get<cb1_t>(v_).consume(n);
        if(buffer_size(boost::get<cb1_t>(v_)) > 0)
            break;
        v_ = boost::blank{};
        if(! more_)
            goto go_complete;
        s_ = do_body + 1;
        break;
    }

    //----------------------------------------------------------------------

    case do_header_c:
        BOOST_ASSERT(n <= buffer_size(
            boost::get<ch0_t>(v_)));
        boost::get<ch0_t>(v_).consume(n);
        if(buffer_size(boost::get<ch0_t>(v_)) > 0)
            break;
        header_done_ = true;
        v_ = boost::blank{};
        if(more_)
            s_ = do_body_c + 1;
        else
            s_ = do_final_c;
        break;

    case do_header_only_c:
    {
        BOOST_ASSERT(n <= buffer_size(
            boost::get<ch_t>(v_)));
        boost::get<ch_t>(v_).consume(n);
        if(buffer_size(boost::get<ch_t>(v_)) > 0)
            break;
        frd_ = boost::none;
        header_done_ = true;
        if(! split_)
        {
            s_ = do_final_c;
            break;
        }
        s_ = do_body_c;
        break;
    }

    case do_body_c + 2:
        BOOST_ASSERT(n <= buffer_size(
            boost::get<ch1_t>(v_)));
        boost::get<ch1_t>(v_).consume(n);
        if(buffer_size(boost::get<ch1_t>(v_)) > 0)
            break;
        v_ = boost::blank{};
        if(more_)
            s_ = do_body_c + 1;
        else
            s_ = do_final_c;
        break;

    case do_final_c + 1:
        BOOST_ASSERT(buffer_size(
            boost::get<ch2_t>(v_)));
        boost::get<ch2_t>(v_).consume(n);
        if(buffer_size(boost::get<ch2_t>(v_)) > 0)
            break;
        v_ = boost::blank{};
        goto go_complete;

    //----------------------------------------------------------------------

    default:
        BOOST_ASSERT(false);
    case do_complete:
        break;

    go_complete:
        s_ = do_complete;
        break;
    }
}

} // http
} // beast

#endif
