#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

find_package(OpenSSL)

# lib-asio
add_library(lib-asio STATIC lib_asio.cpp)
target_compile_definitions(lib-asio PUBLIC
    BOOST_ASIO_SEPARATE_COMPILATION
    BOOST_ASIO_NO_DEPRECATED
    BOOST_ASIO_DISABLE_BOOST_ARRAY
    BOOST_ASIO_DISABLE_BOOST_BIND
    BOOST_ASIO_DISABLE_BOOST_DATE_TIME
    BOOST_ASIO_DISABLE_BOOST_REGEX)
target_link_libraries(lib-asio PUBLIC Boost::asio)
set_target_properties(lib-asio PROPERTIES FOLDER "static-libs")

# lib-asio-ssl
add_library(lib-asio-ssl STATIC lib_asio_ssl.cpp)
target_link_libraries(lib-asio-ssl PUBLIC lib-asio)
target_link_libraries(lib-asio-ssl PUBLIC OpenSSL::SSL OpenSSL::Crypto)
set_target_properties(lib-asio-ssl PROPERTIES FOLDER "static-libs")

# lib-beast
add_library(lib-beast STATIC lib_beast.cpp)
target_compile_definitions(lib-beast PUBLIC BOOST_BEAST_SEPARATE_COMPILATION)
target_link_libraries(lib-beast PUBLIC Boost::beast lib-asio)
set_target_properties(lib-beast PROPERTIES FOLDER "static-libs")

# lib-zlib
enable_language(C)
add_library(lib-zlib STATIC
    extern/zlib-1.2.12/adler32.c
    extern/zlib-1.2.12/compress.c
    extern/zlib-1.2.12/crc32.c
    extern/zlib-1.2.12/deflate.c
    extern/zlib-1.2.12/infback.c
    extern/zlib-1.2.12/inffast.c
    extern/zlib-1.2.12/inflate.c
    extern/zlib-1.2.12/inftrees.c
    extern/zlib-1.2.12/trees.c
    extern/zlib-1.2.12/uncompr.c
    extern/zlib-1.2.12/zutil.c)
target_compile_options(lib-zlib PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-w>
    $<$<CXX_COMPILER_ID:MSVC>:/wd4127>
    $<$<CXX_COMPILER_ID:MSVC>:/wd4244>
    $<$<CXX_COMPILER_ID:MSVC>:/wd4131>)
target_include_directories(lib-zlib PUBLIC extern/zlib-1.2.12)
set_target_properties(lib-zlib PROPERTIES FOLDER "static-libs")

# lib-test
add_library(lib-test STATIC lib_test.cpp)
target_include_directories(lib-test PUBLIC extras/include)
target_link_libraries(lib-test PUBLIC lib-beast)
set_target_properties(lib-test PROPERTIES FOLDER "static-libs")

add_subdirectory(beast)
add_subdirectory(bench)
add_subdirectory(doc)
add_subdirectory(example)

if (BOOST_BEAST_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_subdirectory(fuzz)
endif ()
