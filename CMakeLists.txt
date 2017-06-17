# Part of Beast

cmake_minimum_required (VERSION 3.5.2)

project (Beast VERSION 60)

set_property (GLOBAL PROPERTY USE_FOLDERS ON)

if (MSVC)
    set (CMAKE_VERBOSE_MAKEFILE FALSE)

    add_definitions (-D_WIN32_WINNT=0x0601)
    add_definitions (-D_SCL_SECURE_NO_WARNINGS=1)
    add_definitions (-D_CRT_SECURE_NO_WARNINGS=1)

    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4100 /wd4244 /MP /W4 /bigobj")
    set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ob2 /Oi /Ot /GL")
    set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Oi /Ot")

    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
    set (CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")

    # for RelWithDebInfo builds, disable incremental linking
    # since CMake sets it ON by default for that build type and it
    # causes warnings
    #
    string (REPLACE "/INCREMENTAL" "/INCREMENTAL:NO" replacement_flags
        ${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO})
    set (CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO ${replacement_flags})

else()
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads)

    set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Wpedantic -Wno-unused-parameter")

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wrange-loop-analysis")
    endif ()
endif()

#-------------------------------------------------------------------------------
#
# Boost
#

option (Boost_USE_STATIC_LIBS "Use static libraries for boost" ON)

set (BOOST_COMPONENTS coroutine context filesystem program_options system thread)
find_package (Boost REQUIRED COMPONENTS ${BOOST_COMPONENTS})

if (MINGW)
    link_libraries(ws2_32 mswsock)
endif()

#-------------------------------------------------------------------------------
#
# OpenSSL
#

if (APPLE AND NOT DEFINED ENV{OPENSSL_ROOT_DIR})
  find_program(HOMEBREW brew)
  if (NOT HOMEBREW STREQUAL "HOMEBREW-NOTFOUND")
    execute_process(COMMAND brew --prefix openssl
      OUTPUT_VARIABLE OPENSSL_ROOT_DIR
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()
endif()

find_package(OpenSSL)

if (OPENSSL_FOUND)
    add_definitions (-DBEAST_USE_OPENSSL=1)
else()
    add_definitions (-DBEAST_USE_OPENSSL=0)
    message("OpenSSL not found.")
endif()

#
#-------------------------------------------------------------------------------

function(DoGroupSources curdir rootdir folder)
    file(GLOB children RELATIVE ${PROJECT_SOURCE_DIR}/${curdir} ${PROJECT_SOURCE_DIR}/${curdir}/*)
    foreach(child ${children})
        if(IS_DIRECTORY ${PROJECT_SOURCE_DIR}/${curdir}/${child})
            DoGroupSources(${curdir}/${child} ${rootdir} ${folder})
        elseif(${child} STREQUAL "CMakeLists.txt")
            source_group("" FILES ${PROJECT_SOURCE_DIR}/${curdir}/${child})
        else()
            string(REGEX REPLACE ^${rootdir} ${folder} groupname ${curdir})
            string(REPLACE "/" "\\" groupname ${groupname})
            source_group(${groupname} FILES ${PROJECT_SOURCE_DIR}/${curdir}/${child})
        endif()
    endforeach()
endfunction()

function(GroupSources curdir folder)
    DoGroupSources(${curdir} ${curdir} ${folder})
endfunction()

#-------------------------------------------------------------------------------

if ("${VARIANT}" STREQUAL "coverage")
    set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -DBEAST_NO_BUFFER_BENCH=1 -fprofile-arcs -ftest-coverage")
    set (CMAKE_BUILD_TYPE RELWITHDEBINFO)
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lgcov")

elseif ("${VARIANT}" STREQUAL "ubasan")
    set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -DBEAST_NO_BUFFER_BENCH=1 -funsigned-char -fno-omit-frame-pointer -fsanitize=address,undefined -fsanitize-blacklist=${PROJECT_SOURCE_DIR}/scripts/blacklist.supp")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined")
    set(CMAKE_BUILD_TYPE RELWITHDEBINFO)

elseif ("${VARIANT}" STREQUAL "debug")
    set(CMAKE_BUILD_TYPE DEBUG)

elseif ("${VARIANT}" STREQUAL "release")
    set(CMAKE_BUILD_TYPE RELEASE)

endif()

#-------------------------------------------------------------------------------
#
# Library interface
#

add_library(${PROJECT_NAME} INTERFACE)
target_link_libraries(${PROJECT_NAME} INTERFACE ${Boost_SYSTEM_LIBRARY})
if (NOT MSVC)
    target_link_libraries(${PROJECT_NAME} INTERFACE Threads::Threads)
endif()
set_property(TARGET ${PROJECT_NAME} PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})
set_property(TARGET ${PROJECT_NAME} PROPERTY  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})
target_compile_definitions(${PROJECT_NAME} INTERFACE BOOST_COROUTINES_NO_DEPRECATION_WARNING=1)

#-------------------------------------------------------------------------------
#
# Tests and examples
#

include_directories (.)
include_directories (extras)
include_directories (include)

set(ZLIB_SOURCES
    ${PROJECT_SOURCE_DIR}/test/zlib/zlib-1.2.8/crc32.h
    ${PROJECT_SOURCE_DIR}/test/zlib/zlib-1.2.8/deflate.h
    ${PROJECT_SOURCE_DIR}/test/zlib/zlib-1.2.8/inffast.h
    ${PROJECT_SOURCE_DIR}/test/zlib/zlib-1.2.8/inffixed.h
    ${PROJECT_SOURCE_DIR}/test/zlib/zlib-1.2.8/inflate.h
    ${PROJECT_SOURCE_DIR}/test/zlib/zlib-1.2.8/inftrees.h
    ${PROJECT_SOURCE_DIR}/test/zlib/zlib-1.2.8/trees.h
    ${PROJECT_SOURCE_DIR}/test/zlib/zlib-1.2.8/zlib.h
    ${PROJECT_SOURCE_DIR}/test/zlib/zlib-1.2.8/zutil.h
    ${PROJECT_SOURCE_DIR}/test/zlib/zlib-1.2.8/adler32.c
    ${PROJECT_SOURCE_DIR}/test/zlib/zlib-1.2.8/compress.c
    ${PROJECT_SOURCE_DIR}/test/zlib/zlib-1.2.8/crc32.c
    ${PROJECT_SOURCE_DIR}/test/zlib/zlib-1.2.8/deflate.c
    ${PROJECT_SOURCE_DIR}/test/zlib/zlib-1.2.8/infback.c
    ${PROJECT_SOURCE_DIR}/test/zlib/zlib-1.2.8/inffast.c
    ${PROJECT_SOURCE_DIR}/test/zlib/zlib-1.2.8/inflate.c
    ${PROJECT_SOURCE_DIR}/test/zlib/zlib-1.2.8/inftrees.c
    ${PROJECT_SOURCE_DIR}/test/zlib/zlib-1.2.8/trees.c
    ${PROJECT_SOURCE_DIR}/test/zlib/zlib-1.2.8/uncompr.c
    ${PROJECT_SOURCE_DIR}/test/zlib/zlib-1.2.8/zutil.c
)

file(GLOB_RECURSE BEAST_INCLUDES
    ${PROJECT_SOURCE_DIR}/include/beast/*.hpp
    ${PROJECT_SOURCE_DIR}/include/beast/*.ipp
)

file(GLOB_RECURSE EXAMPLE_INCLUDES
    ${PROJECT_SOURCE_DIR}/example/*.hpp
  )

file(GLOB_RECURSE EXTRAS_INCLUDES
    ${PROJECT_SOURCE_DIR}/extras/beast/*.hpp
    ${PROJECT_SOURCE_DIR}/extras/beast/*.ipp
)

file(GLOB_RECURSE SERVER_INCLUDES
    ${PROJECT_SOURCE_DIR}/example/server-framework/*.hpp
  )

add_subdirectory (test)

add_subdirectory (example)

if (OPENSSL_FOUND)
    add_subdirectory (example/ssl-http-client)
    add_subdirectory (example/ssl-websocket-client)
    add_subdirectory (test/websocket/ssl)
endif()
