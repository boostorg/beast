//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_CORE_DETAIL_CONFIG_HPP
#define BEAST_CORE_DETAIL_CONFIG_HPP

#include <boost/config.hpp>

#if __cplusplus < 201500
# define BEAST_FALLTHROUGH BOOST_FALLTHROUGH
#else
# define BEAST_FALLTHROUGH [[fallthrough]]
#endif

#endif
