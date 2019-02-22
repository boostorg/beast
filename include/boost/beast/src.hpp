//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_SRC_HPP
#define BOOST_BEAST_SRC_HPP

/*

This file is meant to be included once, in a translation unit of
the program, with the macro BOOST_BEAST_SPLIT_COMPILATION defined.

*/

#define BOOST_BEAST_SOURCE

#include <boost/beast/core/detail/config.hpp>

#if defined(BOOST_BEAST_HEADER_ONLY)
# error Do not compile Beast library source with BOOST_BEAST_HEADER_ONLY defined
#endif

#include <boost/beast/_experimental/test/impl/error.ipp>
#include <boost/beast/_experimental/test/impl/stream.ipp>

#include <boost/beast/core/detail/base64.ipp>
#include <boost/beast/core/detail/sha1.ipp>
#include <boost/beast/core/impl/error.ipp>
#include <boost/beast/core/impl/file_posix.ipp>
#include <boost/beast/core/impl/file_stdio.ipp>
#include <boost/beast/core/impl/file_win32.ipp>
#include <boost/beast/core/impl/static_buffer.ipp>

#include <boost/beast/http/impl/error.ipp>
#include <boost/beast/http/impl/field.ipp>
#include <boost/beast/http/impl/status.ipp>
#include <boost/beast/http/impl/verb.ipp>

#include <boost/beast/websocket/detail/prng.ipp>
#include <boost/beast/websocket/impl/error.ipp>

#include <boost/beast/zlib/detail/deflate_stream.ipp>
#include <boost/beast/zlib/detail/inflate_stream.ipp>
#include <boost/beast/zlib/impl/error.ipp>

#endif
