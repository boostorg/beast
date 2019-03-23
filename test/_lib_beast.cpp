//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// This file is used to build a static library with
// asio and beast definitions, to reduce compilation time.

// BOOST_ASIO_SEPARATE_COMPILATION for asio
#include <boost/asio/impl/src.hpp>

// BOOST_BEAST_SPLIT_COMPILATION for beast
#include <boost/beast/src.hpp>
//#include <boost/beast/ssl/impl/src.hpp>

#ifdef BOOST_BEAST_INCLUDE_TEST_MAIN
#include <boost/beast/_experimental/unit_test/main.cpp>
#endif
