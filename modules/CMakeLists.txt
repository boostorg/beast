#
# Copyright (c) 2016-2026 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_library(boost_beast_module)

target_sources(boost_beast_module
    PUBLIC
        FILE_SET CXX_MODULES FILES
            boost_beast.cppm
)

target_compile_features(boost_beast_module PUBLIC cxx_std_20)

target_link_libraries(boost_beast_module PUBLIC Boost::beast)

add_library(Boost::beast_module ALIAS boost_beast_module)
