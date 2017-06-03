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
namespace detail {

template<class Fields>
void
write_start_line(std::ostream& os,
    header<true, Fields> const& msg)
{
    BOOST_ASSERT(msg.version == 10 || msg.version == 11);
    os << msg.method() << " " << msg.target();
    switch(msg.version)
    {
    case 10: os << " HTTP/1.0\r\n"; break;
    case 11: os << " HTTP/1.1\r\n"; break;
    }
}

template<class Fields>
void
write_start_line(std::ostream& os,
    header<false, Fields> const& msg)
{
    BOOST_ASSERT(msg.version == 10 || msg.version == 11);
    switch(msg.version)
    {
    case 10: os << "HTTP/1.0 "; break;
    case 11: os << "HTTP/1.1 "; break;
    }
    auto const reason = msg.reason();
    if(reason.empty())
        os << msg.status << " " << msg.reason() << "\r\n";
    else
        os << msg.status << " " <<
            obsolete_reason(static_cast<status>(msg.status)) << "\r\n";
}

template<class FieldSequence>
void
write_fields(std::ostream& os,
    FieldSequence const& fields)
{
    //static_assert(is_FieldSequence<FieldSequence>::value,
    //    "FieldSequence requirements not met");
    for(auto const& field : fields)
    {
        auto const name = field.name();
        BOOST_ASSERT(! name.empty());
        if(name[0] == ':')
            continue;
        os << field.name() << ": " << field.value() << "\r\n";
    }
}

} // detail

//------------------------------------------------------------------------------

template<bool isRequest, class Body, class Fields,
    class ChunkDecorator, class Allocator>
serializer<isRequest, Body, Fields,
    ChunkDecorator, Allocator>::
serializer(message<isRequest, Body, Fields> const& m,
        ChunkDecorator const& d, Allocator const& alloc)
    : m_(m)
    , d_(d)
    , b_(1024, alloc)
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
        chunked_ = token_list{
            m_.fields["Transfer-Encoding"]}.exists("chunked");
        close_ = token_list{m_.fields["Connection"]}.exists("close") ||
            (m_.version < 11 && ! m_.fields.exists("Content-Length"));
        auto os = ostream(b_);
        detail::write_start_line(os, m_);
        detail::write_fields(os, m_.fields);
        os << "\r\n";
        if(chunked_)
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
            b_.data(),
            result->first};
        s_ = do_header;
        // [[fallthrough]]
    }

    case do_header:
        visit(ec, boost::get<cb0_t>(v_));
        break;

         go_header_only:
    s_ = do_header_only;
    case do_header_only:
        visit(ec, b_.data());
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
            b_.data(),
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
    s_ = do_header_only_c;
    case do_header_only_c:
        visit(ec, b_.data());
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
        b_.consume(b_.size()); // VFALCO delete b_?
        if(! more_)
            goto go_complete;
        s_ = do_body + 1;
        break;

    case do_header_only:
        BOOST_ASSERT(n <= b_.size());
        b_.consume(n);
        if(buffer_size(b_.data()) > 0)
            break;
        // VFALCO delete b_?
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
        b_.consume(b_.size()); // VFALCO delete b_?
        if(more_)
            s_ = do_body_c + 1;
        else
            s_ = do_final_c;
        break;

    case do_header_only_c:
    {
        BOOST_ASSERT(n <= buffer_size(b_.data()));
        b_.consume(n);
        if(buffer_size(b_.data()) > 0)
            break;
        // VFALCO delete b_?
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
