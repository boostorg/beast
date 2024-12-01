#
# Copyright (c) 2022 alandefreitas (alandefreitas@gmail.com)
# Copyright (c) 2023 Christian Mazakas
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#

cmake_minimum_required(VERSION 3.5...3.16)

project(cmake_subdir_test LANGUAGES CXX)
set(__ignore__ ${CMAKE_C_COMPILER})
set(__ignore__ ${CMAKE_C_FLAGS})

if(BOOST_CI_INSTALL_TEST)
  find_package(Boost CONFIG REQUIRED COMPONENTS beast)
else()
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  add_subdirectory(../.. boostorg/beast)

  set(BOOST_BEAST_BUILD_TESTS OFF CACHE BOOL "" FORCE)

  set(deps
  # Primary dependencies

  asio
  assert
  bind
  config
  container
  container_hash
  core
  endian
  intrusive
  logic
  mp11
  optional
  preprocessor
  smart_ptr
  static_assert
  static_string
  system
  throw_exception
  type_index
  type_traits
  winapi

  # Secondary dependencies

  align
  context
  date_time
  move
  describe
  utility
  variant2
  predef
  pool
  algorithm
  io
  lexical_cast
  numeric/conversion
  range
  tokenizer
  array
  concept_check
  exception
  function
  iterator
  mpl
  regex
  tuple
  unordered
  conversion
  integer
  detail
  function_types
  fusion
  functional
  typeof
  )

  foreach(dep IN LISTS deps)
    add_subdirectory(../../../${dep} boostorg/${dep} EXCLUDE_FROM_ALL)
  endforeach()
endif()

add_executable(main main.cpp)
source_group("" FILES main.cpp)
target_link_libraries(main Boost::beast)

enable_testing()
add_test(NAME main COMMAND main)
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>)
