//
// Copyright (c) 2016-2020 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2020 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//
#ifndef BOOST_BEAST_TEST_CONFIG_HPP
#define BOOST_BEAST_TEST_CONFIG_HPP

//
// Configures which tests will run and how

#include <boost/asio/detail/config.hpp>

#if defined(BOOST_ASIO_NO_TS_EXECUTORS)
#define BOOST_BEAST_ENABLE_STACKFUL_TESTS 0
#else
#define BOOST_BEAST_ENABLE_STACKFUL_TESTS 1
#endif

#endif // BOOST_BEAST_TEST_CONFIG_HPP
