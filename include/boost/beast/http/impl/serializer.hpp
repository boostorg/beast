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

template<
    bool isRequest, class Body, class Fields>
template<std::size_t I, class Visit>
inline
void
serializer<isRequest, Body, Fields>::
do_visit(error_code& ec, Visit& visit)
{
    pv_.template emplace<I>(limit_, v_.template get<I>());
    visit(ec, beast::detail::make_buffers_ref(
        pv_.template get<I>()));
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
        v_.template emplace<2>(
            boost::in_place_init,
            fwr_->get(),
            result->first);
        s_ = do_header;
        BOOST_FALLTHROUGH;
    }

    case do_header:
        do_visit<2>(ec, visit);
        break;

    go_header_only:
        v_.template emplace<1>(fwr_->get());
        s_ = do_header_only;
        BOOST_FALLTHROUGH;
    case do_header_only:
        do_visit<1>(ec, visit);
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
        v_.template emplace<3>(result->first);
        s_ = do_body + 2;
        BOOST_FALLTHROUGH;
    }

    case do_body + 2:
        do_visit<3>(ec, visit);
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
            v_.template emplace<7>(
                boost::in_place_init,
                fwr_->get(),
                buffer_bytes(result->first),
                net::const_buffer{nullptr, 0},
                chunk_crlf{},
                result->first,
                chunk_crlf{},
                detail::chunk_last(),
                net::const_buffer{nullptr, 0},
                chunk_crlf{});
            goto go_all_c;
        }
        v_.template emplace<4>(
            boost::in_place_init,
            fwr_->get(),
            buffer_bytes(result->first),
            net::const_buffer{nullptr, 0},
            chunk_crlf{},
            result->first,
            chunk_crlf{});
        s_ = do_header_c;
        BOOST_FALLTHROUGH;
    }

    case do_header_c:
        do_visit<4>(ec, visit);
        break;

    go_header_only_c:
        v_.template emplace<1>(fwr_->get());
        s_ = do_header_only_c;
        BOOST_FALLTHROUGH;

    case do_header_only_c:
        do_visit<1>(ec, visit);
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
            v_.template emplace<6>(
                boost::in_place_init,
                buffer_bytes(result->first),
                net::const_buffer{nullptr, 0},
                chunk_crlf{},
                result->first,
                chunk_crlf{},
                detail::chunk_last(),
                net::const_buffer{nullptr, 0},
                chunk_crlf{});
            goto go_body_final_c;
        }
        v_.template emplace<5>(
            boost::in_place_init,
            buffer_bytes(result->first),
            net::const_buffer{nullptr, 0},
            chunk_crlf{},
            result->first,
            chunk_crlf{});
        s_ = do_body_c + 2;
        BOOST_FALLTHROUGH;
    }

    case do_body_c + 2:
        do_visit<5>(ec, visit);
        break;

    go_body_final_c:
        s_ = do_body_final_c;
        BOOST_FALLTHROUGH;
    case do_body_final_c:
        do_visit<6>(ec, visit);
        break;

    go_all_c:
        s_ = do_all_c;
        BOOST_FALLTHROUGH;
    case do_all_c:
        do_visit<7>(ec, visit);
        break;

    go_final_c:
    case do_final_c:
        v_.template emplace<8>(
            boost::in_place_init,
            detail::chunk_last(),
            net::const_buffer{nullptr, 0},
            chunk_crlf{});
        s_ = do_final_c + 1;
        BOOST_FALLTHROUGH;

    case do_final_c + 1:
        do_visit<8>(ec, visit);
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
std::size_t
serializer<isRequest, Body, Fields>::
consume(std::size_t n)
{
    std::size_t consumed = 0u;
    switch(s_)
    {
    case do_header:
    {
        BOOST_ASSERT(
            n <= buffer_bytes(v_.template get<2>()));
        cb2_t & cb = v_.template get<2>();

        const std::size_t pre_size = buffer_bytes(cb.buffer_sequence().template get<0u>());
        const std::size_t skip = (std::max)(cb.skip(), pre_size); // the start of the consumed data
        cb.consume(n);
        if (cb.skip() > skip)
          consumed += cb.skip() - skip;

        if (cb.skip() > pre_size)
          header_done_ = true;

        if(buffer_bytes(cb) > 0)
            break;

        v_.reset();
        if(! more_)
            goto go_complete;
        s_ = do_body + 1;
        break;
    }
    case do_header_only:
    {
        BOOST_ASSERT(
            n <= buffer_bytes(v_.template get<1>()));
        v_.template get<1>().consume(n);
        if(buffer_bytes(v_.template get<1>()) > 0)
            break;
        fwr_ = boost::none;
        header_done_ = true;
        if(! split_)
            goto go_complete;
        s_ = do_body;
        break;
    }
    case do_body + 2:
    {
        BOOST_ASSERT(
            n <= buffer_bytes(v_.template get<3>()));
        v_.template get<3>().consume(n);
        consumed = n;
        if(buffer_bytes(v_.template get<3>()) > 0)
            break;
        v_.reset();
        if(! more_)
            goto go_complete;
        s_ = do_body + 1;
        break;
    }
    //----------------------------------------------------------------------

    case do_header_c:
    {
        BOOST_ASSERT(
            n <= buffer_bytes(v_.template get<4>()));

        cb4_t& cb = v_.template get<4>();
        const typename cb4_t::sequence_type & bs = cb.buffer_sequence();

        const std::size_t prefix = buffer_bytes(bs.template get<0>())
                                 + buffer_bytes(bs.template get<1>())
                                 + buffer_bytes(bs.template get<2>())
                                 + buffer_bytes(bs.template get<3>());

        const std::size_t suffix = buffer_bytes(bs.template get<5>());
        const std::size_t skip = (std::max)(cb.skip(), prefix);

        cb.consume(n);

        const std::size_t consume_end = (std::min)(buffer_bytes(bs) - suffix, cb.skip());

        if (cb.skip() > skip)
            consumed += consume_end - skip;

        if (cb.skip() >= buffer_bytes(bs.template get<0>()))
            header_done_ = true;

        if(buffer_bytes(cb) > 0)
            break;
        v_.reset();
        if(more_)
            s_ = do_body_c + 1;
        else
            s_ = do_final_c;
        break;
    }
    case do_header_only_c:
    {
        BOOST_ASSERT(
            n <= buffer_bytes(v_.template get<1>()));
        v_.template get<1>().consume(n);
        if(buffer_bytes(v_.template get<1>()) > 0)
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
    {
        BOOST_ASSERT(
            n <= buffer_bytes(v_.template get<5>()));

        cb5_t& cb = v_.template get<5>();
        const typename cb5_t::sequence_type & bs = cb.buffer_sequence();

        const std::size_t prefix = buffer_bytes(bs.template get<0>())
                                 + buffer_bytes(bs.template get<1>())
                                 + buffer_bytes(bs.template get<2>());

        const std::size_t suffix = buffer_bytes(bs.template get<4>());
        const std::size_t skip = (std::max)(cb.skip(), prefix);

        cb.consume(n);

        const std::size_t consume_end = (std::min)({buffer_bytes(bs), suffix, cb.skip()});
        if (cb.skip() > skip)
            consumed += consume_end - skip;

        if(buffer_bytes(cb) > 0)
            break;
        v_.reset();
        if(more_)
            s_ = do_body_c + 1;
        else
            s_ = do_final_c;
        break;

    }
    case do_body_final_c:
    {
        BOOST_ASSERT(
            n <= buffer_bytes(v_.template get<6>()));

        cb6_t& cb = v_.template get<6>();
        const typename cb6_t::sequence_type & bs = cb.buffer_sequence();

        const std::size_t prefix = buffer_bytes(bs.template get<0>())
                                 + buffer_bytes(bs.template get<1>())
                                 + buffer_bytes(bs.template get<2>());

        const std::size_t suffix = buffer_bytes(bs.template get<4>())
                                 + buffer_bytes(bs.template get<5>())
                                 + buffer_bytes(bs.template get<6>())
                                 + buffer_bytes(bs.template get<7>());

        const std::size_t skip = (std::max)(cb.skip(), prefix);

        cb.consume(n);

        const std::size_t consume_end = (std::min)({buffer_bytes(bs), suffix, cb.skip()});
        if (cb.skip() > skip)
            consumed += consume_end - skip;

        if(buffer_bytes(cb) > 0)
            break;
        v_.reset();
        s_ = do_complete;
        break;
    }

    case do_all_c:
    {
        BOOST_ASSERT(
            n <= buffer_bytes(v_.template get<7>()));

        cb7_t& cb = v_.template get<7>();
        const typename cb7_t::sequence_type & bs = cb.buffer_sequence();

        const std::size_t prefix = buffer_bytes(bs.template get<0>())
                                 + buffer_bytes(bs.template get<1>())
                                 + buffer_bytes(bs.template get<2>())
                                 + buffer_bytes(bs.template get<3>());

        const std::size_t suffix = buffer_bytes(bs.template get<5>())
                                 + buffer_bytes(bs.template get<6>())
                                 + buffer_bytes(bs.template get<7>())
                                 + buffer_bytes(bs.template get<8>());

        const std::size_t skip = (std::max)(cb.skip(), prefix);

        cb.consume(n);

        const std::size_t consume_end = (std::min)({buffer_bytes(bs), suffix, cb.skip()});
        if (cb.skip() > skip)
            consumed += consume_end - skip;

        if (cb.skip() >= buffer_bytes(bs.template get<0>()))
            header_done_ = true;

        if(buffer_bytes(cb) > 0)
            break;

        v_.reset();
        s_ = do_complete;
        break;
    }

    case do_final_c + 1:
        BOOST_ASSERT(buffer_bytes(v_.template get<8>()));
        v_.template get<8>().consume(n);
        if(buffer_bytes(v_.template get<8>()) > 0)
            break;
        v_.reset();
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
    return consumed;
}

} // http
} // beast
} // boost

#endif
