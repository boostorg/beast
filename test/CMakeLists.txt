#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

include_directories (./extern)
include_directories (./extras/include)

file (GLOB_RECURSE EXTRAS_FILES
    ${PROJECT_SOURCE_DIR}/test/extras/include/*.hpp
    ${PROJECT_SOURCE_DIR}/test/extras/include/*.ipp
    ${PROJECT_SOURCE_DIR}/test/lib_ssl.cpp
    )

if (OPENSSL_FOUND)
    link_libraries (${OPENSSL_LIBRARIES})
endif()

set (ZLIB_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/crc32.h
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/deflate.h
   #${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/gzguts.h
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/inffast.h
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/inffixed.h
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/inflate.h
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/inftrees.h
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/trees.h
   #${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/zconf.h
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/zlib.h
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/zutil.h
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/adler32.c
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/compress.c
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/crc32.c
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/deflate.c
   #${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/gzclose.c
   #${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/gzlib.c
   #${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/gzread.c
   #${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/gzwrite.c
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/infback.c
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/inffast.c
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/inflate.c
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/inftrees.c
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/trees.c
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/uncompr.c
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12/zutil.c
)

add_library (
        lib-zlib
        ${ZLIB_SOURCES}
)
target_include_directories(lib-zlib PUBLIC "${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.12")
set_property(TARGET lib-zlib PROPERTY FOLDER "static-libs")

set (TEST_MAIN ${PROJECT_SOURCE_DIR}/test/lib_beast.cpp)

GroupSources (. "/")

add_library (
    lib-test STATIC
    ${CMAKE_CURRENT_LIST_DIR}/lib_test.cpp
)

set_property(TARGET lib-test PROPERTY FOLDER "static-libs")

add_subdirectory (beast)
add_subdirectory (bench)
add_subdirectory (doc)
add_subdirectory (example)
if (Beast_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_subdirectory(fuzz)
endif()
