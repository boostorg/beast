//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_IMPL_SERIALIZER_HPP
#define BOOST_BEAST_HTTP_IMPL_SERIALIZER_HPP

#include <boost/beast/core/buffer_traits.hpp>
#include <boost/beast/core/detail/buffers_ref.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/core/detail/config.hpp>
#include <boost/assert.hpp>
#include <ostream>

namespace boost {
namespace beast {
namespace http {

namespace detail {
template<class Visitor>
void
do_visit2(
    Visitor& v,
    beast::detail::polymorphic_const_buffer_sequence const& buffers,
    std::size_t limit,
    error_code& ec)
{
    v(ec, buffers.prefix_copy(limit));
}
}

template<
    bool isRequest, class Body, class Fields>
void
serializer<isRequest, Body, Fields>::
fwrinit(std::true_type)
{
    fwr_.emplace(m_, m_.version(), m_.method());
}

template<
    bool isRequest, class Body, class Fields>
void
serializer<isRequest, Body, Fields>::
fwrinit(std::false_type)
{
    fwr_.emplace(m_, m_.version(), m_.result_int());
}

//------------------------------------------------------------------------------

template<
    bool isRequest, class Body, class Fields>
serializer<isRequest, Body, Fields>::
serializer(value_type& m)
    : m_(m)
    , wr_(m_.base(), m_.body())
{
}

template<
    bool isRequest, class Body, class Fields>
template<class Visit>
void
serializer<isRequest, Body, Fields>::
next(error_code& ec, Visit&& visit)
{
    switch(s_)
    {
    case do_construct:
    {
        fwrinit(std::integral_constant<bool,
            isRequest>{});
        if(m_.chunked())
            goto go_init_c;
        s_ = do_init;
        BOOST_FALLTHROUGH;
    }

    case do_init:
    {
        wr_.init(ec);
        if(ec)
            return;
        if(split_)
            goto go_header_only;
        auto result = wr_.get(ec);
        if(ec == error::need_more)
            goto go_header_only;
        if(ec)
            return;
        if(! result)
            goto go_header_only;
        more_ = result->second;
        cbs_
        .clear()
        .append(fwr_->get())
        .append(result->first);
        s_ = do_header;
        BOOST_FALLTHROUGH;
    }

    case do_header:
        detail::do_visit2(visit, cbs_, limit_, ec);
        break;

    go_header_only:
        cbs_.clear().append(fwr_->get());
        s_ = do_header_only;
        BOOST_FALLTHROUGH;
    case do_header_only:
        detail::do_visit2(visit, cbs_, limit_, ec);
        break;

    case do_body:
        s_ = do_body + 1;
        BOOST_FALLTHROUGH;

    case do_body + 1:
    {
        auto result = wr_.get(ec);
        if(ec)
            return;
        if(! result)
            goto go_complete;
        more_ = result->second;
        cbs_.clear().append(result->first);
        s_ = do_body + 2;
        BOOST_FALLTHROUGH;
    }

    case do_body + 2:
        detail::do_visit2(visit, cbs_, limit_, ec);
        break;

    //----------------------------------------------------------------------

        go_init_c:
        s_ = do_init_c;
        BOOST_FALLTHROUGH;
    case do_init_c:
    {
        wr_.init(ec);
        if(ec)
            return;
        if(split_)
            goto go_header_only_c;
        auto result = wr_.get(ec);
        if(ec == error::need_more)
            goto go_header_only_c;
        if(ec)
            return;
        if(! result)
            goto go_header_only_c;
        more_ = result->second;
        if(! more_)
        {
            // do it all in one buffer
            cbs_.clear()
            .append(fwr_->get())
            .append(cs_ = detail::chunk_size(buffer_bytes(result->first)))
            .append(chunk_crlf())
            .append(result->first)
            .append(chunk_crlf())
            .append(detail::chunk_last())
            .append(chunk_crlf());
            goto go_all_c;
        }
        cbs_.clear()
        .append(fwr_->get())
        .append(cs_ = detail::chunk_size(buffer_bytes(result->first)))
        .append(chunk_crlf())
        .append(result->first)
        .append(chunk_crlf());
        s_ = do_header_c;
        BOOST_FALLTHROUGH;
    }

    case do_header_c:
        detail::do_visit2(visit, cbs_, limit_, ec);
        break;

    go_header_only_c:
        cbs_.clear().append(fwr_->get());
        s_ = do_header_only_c;
        BOOST_FALLTHROUGH;

    case do_header_only_c:
        detail::do_visit2(visit, cbs_, limit_, ec);
        break;

    case do_body_c:
        s_ = do_body_c + 1;
        BOOST_FALLTHROUGH;

    case do_body_c + 1:
    {
        auto result = wr_.get(ec);
        if(ec)
            return;
        if(! result)
            goto go_final_c;
        more_ = result->second;
        if(! more_)
        {
            // do it all in one buffer
            cbs_.clear()
            .append(cs_ = detail::chunk_size(buffer_bytes(result->first)))
            .append(chunk_crlf())
            .append(result->first)
            .append(chunk_crlf())
            .append(detail::chunk_last())
            .append(chunk_crlf());
            goto go_body_final_c;
        }
        cbs_.clear()
        .append(cs_ = detail::chunk_size(buffer_bytes(result->first)))
        .append(chunk_crlf())
        .append(result->first)
        .append(chunk_crlf());
        s_ = do_body_c + 2;
        BOOST_FALLTHROUGH;
    }

    case do_body_c + 2:
        detail::do_visit2(visit, cbs_, limit_, ec);
        break;

    go_body_final_c:
        s_ = do_body_final_c;
        BOOST_FALLTHROUGH;
    case do_body_final_c:
        detail::do_visit2(visit, cbs_, limit_, ec);
        break;

    go_all_c:
        s_ = do_all_c;
        BOOST_FALLTHROUGH;
    case do_all_c:
        detail::do_visit2(visit, cbs_, limit_, ec);
        break;

    go_final_c:
    case do_final_c:
        cbs_.clear()
        .append(detail::chunk_last())
        .append(chunk_crlf{});
        s_ = do_final_c + 1;
        BOOST_FALLTHROUGH;

    case do_final_c + 1:
        detail::do_visit2(visit, cbs_, limit_, ec);
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

template<
    bool isRequest, class Body, class Fields>
void
serializer<isRequest, Body, Fields>::
consume(std::size_t n)
{
    switch(s_)
    {
    case do_header:
        BOOST_ASSERT(
            n <= buffer_bytes(cbs_));
        cbs_.consume(n);
        if(buffer_bytes(cbs_) > 0)
            break;
        header_done_ = true;
        cbs_.clear();
        if(! more_)
            goto go_complete;
        s_ = do_body + 1;
        break;

    case do_header_only:
        BOOST_ASSERT(
            n <= buffer_bytes(cbs_));
        cbs_.consume(n);
        if(buffer_bytes(cbs_) > 0)
            break;
        fwr_ = boost::none;
        header_done_ = true;
        if(! split_)
            goto go_complete;
        s_ = do_body;
        break;

    case do_body + 2:
    {
        BOOST_ASSERT(
            n <= buffer_bytes(cbs_));
        cbs_.consume(n);
        if(buffer_bytes(cbs_) > 0)
            break;
        cbs_.clear();
        if(! more_)
            goto go_complete;
        s_ = do_body + 1;
        break;
    }

    //----------------------------------------------------------------------

    case do_header_c:
        BOOST_ASSERT(
            n <= buffer_bytes(cbs_));
        cbs_.consume(n);
        if(buffer_bytes(cbs_) > 0)
            break;
        header_done_ = true;
        cbs_.clear();

        if(more_)
            s_ = do_body_c + 1;
        else
            s_ = do_final_c;
        break;

    case do_header_only_c:
    {
        BOOST_ASSERT(
            n <= buffer_bytes(cbs_));
        cbs_.consume(n);
        if(buffer_bytes(cbs_) > 0)
            break;
        fwr_ = boost::none;
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
        BOOST_ASSERT(
            n <= buffer_bytes(cbs_));
        cbs_.consume(n);
        if(buffer_bytes(cbs_) > 0)
            break;
        cbs_.clear();
        if(more_)
            s_ = do_body_c + 1;
        else
            s_ = do_final_c;
        break;

    case do_body_final_c:
    {
        BOOST_ASSERT(
            n <= buffer_bytes(cbs_));
        cbs_.consume(n);
        if(buffer_bytes(cbs_) > 0)
            break;
        cbs_.clear();
        s_ = do_complete;
        break;
    }

    case do_all_c:
    {
        BOOST_ASSERT(
            n <= buffer_bytes(cbs_));
        cbs_.consume(n);
        if(buffer_bytes(cbs_) > 0)
            break;
        header_done_ = true;
        cbs_.clear();
        s_ = do_complete;
        break;
    }

    case do_final_c + 1:
        BOOST_ASSERT(buffer_bytes(cbs_));
        cbs_.consume(n);
        if(buffer_bytes(cbs_) > 0)
            break;
        cbs_.clear();
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
} // boost

#endif
