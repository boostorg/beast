#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_executable(http-crawl
    Jamfile
    http_crawl.cpp
    urls_large_data.cpp)

source_group("" FILES
    Jamfile
    http_crawl.cpp
    urls_large_data.cpp)

target_link_libraries(http-crawl PRIVATE Boost::beast)

set_target_properties(http-crawl
    PROPERTIES FOLDER "example-http-client")
