//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_PARSER_IPP
#define BEAST_HTTP_IMPL_PARSER_IPP

#include <boost/throw_exception.hpp>
#include <stdexcept>

namespace beast {
namespace http {

template<bool isRequest, class Body, class Allocator>
parser<isRequest, Body, Allocator>::
~parser()
{
    if(cb_h_)
        cb_h_->~cb_h_t();
    if(cb_b_)
        cb_b_->~cb_b_t();
}

template<bool isRequest, class Body, class Allocator>
parser<isRequest, Body, Allocator>::
parser()
    : wr_(m_)
{
}

template<bool isRequest, class Body, class Allocator>
parser<isRequest, Body, Allocator>::
parser(parser&& other)
    : base_type(std::move(other))
    , m_(other.m_)
    , wr_(other.wr_)
    , wr_inited_(other.wr_inited_)
{
    if(other.cb_h_)
        cb_h_ = other.cb_h_->move(
            &cb_h_buf_);
    if(other.cb_b_)
        cb_b_ = other.cb_h_->move(
            &cb_b_buf_);
}

template<bool isRequest, class Body, class Allocator>
template<class Arg1, class... ArgN, class>
parser<isRequest, Body, Allocator>::
parser(Arg1&& arg1, ArgN&&... argn)
    : m_(std::forward<Arg1>(arg1),
        std::forward<ArgN>(argn)...)
    , wr_(m_)
{
}

template<bool isRequest, class Body, class Allocator>
template<class OtherBody, class... Args, class>
parser<isRequest, Body, Allocator>::
parser(parser<isRequest, OtherBody, Allocator>&& other,
        Args&&... args)
    : base_type(std::move(other))
    , m_(other.release(), std::forward<Args>(args)...)
    , wr_(m_)
{
    if(other.wr_inited_)
        BOOST_THROW_EXCEPTION(std::invalid_argument{
            "moved-from parser has a body"});
}

template<bool isRequest, class Body, class Allocator>
template<class Callback>
void
parser<isRequest, Body, Allocator>::
on_chunk_header(Callback& cb)
{
    // Callback may not be constant, caller is responsible for
    // managing the lifetime of the callback. Copies are not made.
    BOOST_STATIC_ASSERT(! std::is_const<Callback>::value);

    // Can't set the callback after receiving any chunk data!
    BOOST_ASSERT(! wr_inited_);

    if(cb_h_)
        cb_h_->~cb_h_t();
    cb_h_ = new(&cb_h_buf_) cb_h_t_impl<Callback>(cb);
}

template<bool isRequest, class Body, class Allocator>
template<class Callback>
void
parser<isRequest, Body, Allocator>::
on_chunk_body(Callback& cb)
{
    // Callback may not be constant, caller is responsible for
    // managing the lifetime of the callback. Copies are not made.
    BOOST_STATIC_ASSERT(! std::is_const<Callback>::value);

    // Can't set the callback after receiving any chunk data!
    BOOST_ASSERT(! wr_inited_);

    if(cb_b_)
        cb_b_->~cb_b_t();
    cb_b_ = new(&cb_b_buf_) cb_b_t_impl<Callback>(cb);
}

} // http
} // beast

#endif
