#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_subdirectory (core)
add_subdirectory (http)
add_subdirectory (websocket)
add_subdirectory (wstest)
add_subdirectory (zlib)

if ((NOT "${VARIANT}" STREQUAL "coverage") AND
    (NOT "${VARIANT}" STREQUAL "ubasan"))

    add_subdirectory (benchmarks)
    add_subdirectory (common)
    add_subdirectory (server)

    GroupSources(extras/boost/beast extras)
    GroupSources(include/boost/beast beast)
    
    GroupSources(test "/")

    add_executable (lib-tests
        ${BOOST_BEAST_INCLUDES}
        ${EXTRAS_INCLUDES}
        Jamfile
        ../extras/boost/beast/unit_test/main.cpp
        config.cpp
        core.cpp
        exemplars.cpp
        http.cpp
        version.cpp
        websocket.cpp
        zlib.cpp
    )

    if (MINGW)
        set_target_properties(lib-tests PROPERTIES COMPILE_FLAGS "-Wa,-mbig-obj")
    endif()
endif()
