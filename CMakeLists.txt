#
# Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
# Copyright (c) 2021 DMitry Arkhipov (grisumbras@gmail.com)
# Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

#-------------------------------------------------
#
# Project
#
#-------------------------------------------------
cmake_minimum_required(VERSION 3.8...3.20)
set(BOOST_BEAST_VERSION 357)
if (BOOST_SUPERPROJECT_VERSION)
    set(BOOST_BEAST_VERSION ${BOOST_SUPERPROJECT_VERSION})
endif ()
project(boost_beast VERSION "${BOOST_BEAST_VERSION}" LANGUAGES CXX)
set(BOOST_BEAST_IS_ROOT OFF)
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(BOOST_BEAST_IS_ROOT ON)
endif ()
set(__ignore__ ${CMAKE_C_COMPILER})

#-------------------------------------------------
#
# Options
#
#-------------------------------------------------
if (BOOST_BEAST_IS_ROOT)
    include(CTest)
endif ()
option(BOOST_BEAST_BUILD_TESTS "Build boost::beast tests" ${BUILD_TESTING})
option(BOOST_BEAST_BUILD_FUZZERS "Build boost::beast fuzzers" OFF)
option(BOOST_BEAST_BUILD_EXAMPLES "Build boost::beast examples" ${BOOST_BEAST_IS_ROOT})
option(BOOST_BEAST_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

# Check if environment variable BOOST_SRC_DIR is set
if (NOT DEFINED BOOST_SRC_DIR AND DEFINED ENV{BOOST_SRC_DIR})
    set(DEFAULT_BOOST_SRC_DIR "$ENV{BOOST_SRC_DIR}")
else ()
    set(DEFAULT_BOOST_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
endif ()
set(BOOST_SRC_DIR ${DEFAULT_BOOST_SRC_DIR} CACHE STRING "Boost source dir to use when running CMake from this directory")

#-------------------------------------------------
#
# Boost modules
#
#-------------------------------------------------
# The boost super-project requires one explicit dependency per-line.
set(BOOST_BEAST_DEPENDENCIES
    Boost::asio
    Boost::assert
    Boost::bind
    Boost::config
    Boost::container
    Boost::container_hash
    Boost::core
    Boost::endian
    Boost::intrusive
    Boost::logic
    Boost::mp11
    Boost::optional
    Boost::preprocessor
    Boost::smart_ptr
    Boost::static_assert
    Boost::static_string
    Boost::system
    Boost::throw_exception
    Boost::type_index
    Boost::type_traits
    Boost::winapi)

foreach (BOOST_BEAST_DEPENDENCY ${BOOST_BEAST_DEPENDENCIES})
    if (BOOST_BEAST_DEPENDENCY MATCHES "^[ ]*Boost::([A-Za-z0-9_]+)[ ]*$")
        list(APPEND BOOST_BEAST_INCLUDE_LIBRARIES ${CMAKE_MATCH_1})
    endif ()
endforeach ()
# Conditional dependencies
if (BOOST_BEAST_BUILD_TESTS)
    set(BOOST_BEAST_UNIT_TEST_LIBRARIES filesystem)
endif ()
if (BOOST_BEAST_BUILD_EXAMPLES)
    set(BOOST_BEAST_EXAMPLE_LIBRARIES json)
endif ()
# Complete dependency list
set(BOOST_INCLUDE_LIBRARIES ${BOOST_BEAST_INCLUDE_LIBRARIES} ${BOOST_BEAST_UNIT_TEST_LIBRARIES} ${BOOST_BEAST_EXAMPLE_LIBRARIES})
set(BOOST_EXCLUDE_LIBRARIES beast)

#-------------------------------------------------
#
# Add Boost Subdirectory
#
#-------------------------------------------------
if (BOOST_BEAST_IS_ROOT)
    set(CMAKE_FOLDER Dependencies)
    # Find absolute BOOST_SRC_DIR
    if (NOT IS_ABSOLUTE ${BOOST_SRC_DIR})
        set(BOOST_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${BOOST_SRC_DIR}")
    endif ()

    # Validate BOOST_SRC_DIR
    set(BOOST_SRC_DIR_IS_VALID ON)
    foreach (F "CMakeLists.txt" "Jamroot" "boost-build.jam" "bootstrap.sh" "libs")
        if (NOT EXISTS "${BOOST_SRC_DIR}/${F}")
            message(STATUS "${BOOST_SRC_DIR}/${F} does not exist. Fallback to find_package.")
            set(BOOST_SRC_DIR_IS_VALID OFF)
            break()
        endif ()
    endforeach ()

    # Create Boost interface targets
    if (BOOST_SRC_DIR_IS_VALID)
        # From BOOST_SRC_DIR
        if (BUILD_SHARED_LIBS)
            set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
        endif ()
        set(BOOST_EXCLUDE_LIBRARIES ${PROJECT_NAME})
        set(PREV_BUILD_TESTING ${BUILD_TESTING})
        set(BUILD_TESTING OFF CACHE BOOL "Build the tests." FORCE)
        add_subdirectory(${BOOST_SRC_DIR} Dependencies/boost EXCLUDE_FROM_ALL)
        set(BUILD_TESTING ${PREV_BUILD_TESTING} CACHE BOOL "Build the tests." FORCE)
        set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${BOOST_SRC_DIR}/tools/cmake/include")
    else ()
        # From Boost Package
        find_package(Boost REQUIRED COMPONENTS filesystem scope json)
        foreach (BOOST_INCLUDE_LIBRARY ${BOOST_INCLUDE_LIBRARIES})
            if (NOT TARGET Boost::${BOOST_INCLUDE_LIBRARY})
                add_library(Boost::${BOOST_INCLUDE_LIBRARY} ALIAS Boost::headers)
            endif ()
        endforeach ()
    endif ()
    unset(CMAKE_FOLDER)
endif ()

#-------------------------------------------------
#
# Library
#
#-------------------------------------------------
add_library(boost_beast INTERFACE)
add_library(Boost::beast ALIAS boost_beast)
target_include_directories(boost_beast INTERFACE include)
target_link_libraries(boost_beast INTERFACE ${BOOST_BEAST_DEPENDENCIES})
target_compile_features(boost_beast INTERFACE cxx_std_11)

if (BOOST_BEAST_IS_ROOT)
    file(GLOB_RECURSE BOOST_BEAST_HEADERS CONFIGURE_DEPENDS include/boost/*.hpp)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include/boost PREFIX "" FILES ${BOOST_BEAST_HEADERS})
    target_sources(boost_beast PRIVATE ${BOOST_BEAST_HEADERS} build.jam)
endif ()
#-------------------------------------------------
#
# Tests
#
#-------------------------------------------------
if (BOOST_BEAST_BUILD_TESTS)
    add_subdirectory(test)
endif ()

#-------------------------------------------------
#
# Examples
#
#-------------------------------------------------
if (BOOST_BEAST_BUILD_EXAMPLES)
    add_subdirectory(example)
endif ()
