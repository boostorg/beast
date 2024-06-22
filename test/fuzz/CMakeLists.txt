#
# Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
# Copyright (c) 2024 Mohammad Nejati
#
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#

# Get number of cores
include(ProcessorCount)
ProcessorCount(PROCESSOR_COUNT)

# Determine total fuzz time per file
file(GLOB BOOST_BEAST_FUZZER_SOURCE_FILES *.cpp)
list(LENGTH BOOST_BEAST_FUZZER_SOURCE_FILES BOOST_BEAST_FUZZER_SOURCE_FILES_COUNT)
set(BOOST_BEAST_FUZZER_TOTAL_TIME_DEFAULT 300)
math(EXPR BOOST_BEAST_FUZZER_TOTAL_TIME_DEFAULT "${BOOST_BEAST_FUZZER_TOTAL_TIME_DEFAULT} / (${PROCESSOR_COUNT} * ${BOOST_BEAST_FUZZER_SOURCE_FILES_COUNT}) + 1")

# Fuzzing options
set(BOOST_BEAST_FUZZER_TOTAL_TIME ${BOOST_BEAST_FUZZER_TOTAL_TIME_DEFAULT} CACHE STRING "Total time for fuzzing")
set(BOOST_BEAST_FUZZER_RSS_LIMIT 8192 CACHE STRING "RSS limit for fuzzing")
set(BOOST_BEAST_FUZZER_TIMEOUT 30 CACHE STRING "Timeout for fuzzing")
set(BOOST_BEAST_FUZZER_MAX_LEN 4000 CACHE STRING "Maximum size of the input")
set(BOOST_BEAST_FUZZER_JOBS ${PROCESSOR_COUNT} CACHE STRING "Number of jobs for fuzzing")
option(BOOST_BEAST_FUZZER_ADD_TO_CTEST "Add fuzzing targets to ctest" OFF)
set(BOOST_BEAST_FUZZER_CORPUS_PATH ${CMAKE_CURRENT_BINARY_DIR}/corpus.tar CACHE STRING "Path to corpus.tar")

# Corpus
set(BOOST_BEAST_FUZZER_SEEDS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/seeds.tar)
set(BOOST_BEAST_FUZZER_SEEDS_DIR ${CMAKE_CURRENT_BINARY_DIR}/seeds)
get_filename_component(BOOST_BEAST_FUZZER_SEEDS_PARENT_DIR ${BOOST_BEAST_FUZZER_SEEDS_DIR} DIRECTORY)
add_custom_target(
    untar_seeds
    COMMAND ${CMAKE_COMMAND} -E echo "Untar fuzz seeds from ${BOOST_BEAST_FUZZER_SEEDS_PATH} to ${BOOST_BEAST_FUZZER_SEEDS_PARENT_DIR}/seeds"
    COMMAND ${CMAKE_COMMAND} -E tar xf ${BOOST_BEAST_FUZZER_SEEDS_PATH}
    WORKING_DIRECTORY ${BOOST_BEAST_FUZZER_SEEDS_PARENT_DIR}
    COMMENT "Unzipping fuzz seeds"
    VERBATIM)

set(BOOST_BEAST_FUZZER_CORPUS_DIR ${CMAKE_CURRENT_BINARY_DIR}/corpus)
set(BOOST_BEAST_FUZZER_MERGED_CORPUS_DIR ${CMAKE_CURRENT_BINARY_DIR}/merged-corpus)
if(EXISTS ${BOOST_BEAST_FUZZER_CORPUS_PATH})
    add_custom_target(
        untar_corpus
        COMMAND ${CMAKE_COMMAND} -E echo "Untar fuzz corpus archive from \"${BOOST_BEAST_FUZZER_CORPUS_PATH}\" to \"${CMAKE_CURRENT_BINARY_DIR}/corpus\""
        COMMAND ${CMAKE_COMMAND} -E tar xf ${BOOST_BEAST_FUZZER_CORPUS_PATH}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Unzipping fuzz corpus"
        VERBATIM)
else()
    add_custom_target(untar_corpus
        COMMAND ${CMAKE_COMMAND} -E echo "No fuzz corpus archive in ${BOOST_BEAST_FUZZER_CORPUS_PATH}. Create empty fuzz corpus dir \"${BOOST_BEAST_FUZZER_CORPUS_DIR}.\""
        COMMAND ${CMAKE_COMMAND} -E make_directory ${BOOST_BEAST_FUZZER_CORPUS_DIR}
        COMMENT "Creating fuzz corpus directory"
        VERBATIM)
endif()
add_dependencies(untar_corpus untar_seeds)

# Target that runs all fuzz targets
get_filename_component(BOOST_BEAST_FUZZER_CORPUS_PARENT_DIR ${BOOST_BEAST_FUZZER_CORPUS_PATH} DIRECTORY)
add_custom_target(
    boost_beast_fuzz_all
    COMMAND ${CMAKE_COMMAND} -E echo "Archive corpus from \"${BOOST_BEAST_FUZZER_CORPUS_DIR}\" to \"${BOOST_BEAST_FUZZER_CORPUS_PATH}\""
    COMMAND ${CMAKE_COMMAND} -E tar cf ${BOOST_BEAST_FUZZER_CORPUS_PATH} ${BOOST_BEAST_FUZZER_CORPUS_DIR}
    WORKING_DIRECTORY ${BOOST_BEAST_FUZZER_CORPUS_PARENT_DIR}
    VERBATIM)

# Register a single fuzzer and add as dependency to fuzz target
function(add_boost_beast_fuzzer NAME)
    # Fuzzer executable
    set(SOURCE_FILES ${ARGN})
    add_executable(fuzzer_${NAME} ${SOURCE_FILES})
    target_link_libraries(fuzzer_${NAME} PRIVATE lib-beast)
    target_compile_options(fuzzer_${NAME} PRIVATE -g -O2 -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined)
    target_link_libraries(fuzzer_${NAME} PRIVATE -fsanitize=fuzzer,address,undefined)
    set_property(TARGET fuzzer_${NAME} PROPERTY FOLDER "fuzzing")

    # Custom target to run fuzzer executable
    add_custom_target(
        fuzz_${NAME}
        ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory ${BOOST_BEAST_FUZZER_CORPUS_DIR}/${NAME}
        COMMAND ${CMAKE_COMMAND} -E echo "Running fuzzer ${NAME} with corpus from ${BOOST_BEAST_FUZZER_CORPUS_DIR}/${NAME} and seeds from ${BOOST_BEAST_FUZZER_SEEDS_DIR}/${NAME}"
        COMMAND
            fuzzer_${NAME}
            -rss_limit_mb=${BOOST_BEAST_FUZZER_RSS_LIMIT}
            -max_total_time=${BOOST_BEAST_FUZZER_TOTAL_TIME}
            -timeout=${BOOST_BEAST_FUZZER_TIMEOUT}
            -max_len=${BOOST_BEAST_FUZZER_MAX_LEN}
            -jobs=${BOOST_BEAST_FUZZER_JOBS}
            ${BOOST_BEAST_FUZZER_CORPUS_DIR}/${NAME}
            ${BOOST_BEAST_FUZZER_SEEDS_DIR}/${NAME}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${BOOST_BEAST_FUZZER_MERGED_CORPUS_DIR}/${NAME}
        COMMAND ${CMAKE_COMMAND} -E echo "Merging corpus from ${BOOST_BEAST_FUZZER_CORPUS_DIR}/${NAME} to ${BOOST_BEAST_FUZZER_MERGED_CORPUS_DIR}/${NAME}"
        COMMAND
            fuzzer_${NAME}
            -merge=1
            ${BOOST_BEAST_FUZZER_MERGED_CORPUS_DIR}/${NAME}
            ${BOOST_BEAST_FUZZER_CORPUS_DIR}/${NAME}
            ${BOOST_BEAST_FUZZER_SEEDS_DIR}/${NAME}
        COMMAND ${CMAKE_COMMAND} -E echo "Replacing corpus in ${BOOST_BEAST_FUZZER_CORPUS_DIR}/${NAME} with merged corpus from ${BOOST_BEAST_FUZZER_MERGED_CORPUS_DIR}/${NAME}"
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${BOOST_BEAST_FUZZER_CORPUS_DIR}/${NAME}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${BOOST_BEAST_FUZZER_CORPUS_DIR}/${NAME}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${BOOST_BEAST_FUZZER_MERGED_CORPUS_DIR}/${NAME} ${BOOST_BEAST_FUZZER_CORPUS_DIR}/${NAME}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${BOOST_BEAST_FUZZER_MERGED_CORPUS_DIR}/${NAME}
        DEPENDS untar_corpus fuzzer_${NAME})
    add_dependencies(fuzz_${NAME} fuzzer_${NAME})
    add_dependencies(boost_beast_fuzz_all fuzz_${NAME})
    set_property(TARGET fuzz_${NAME} PROPERTY ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=false")
endfunction()

# Register all fuzzers
file(GLOB BOOST_BEAST_FUZZER_SOURCE_FILES *.cpp)
source_group("" FILES ${BOOST_BEAST_FUZZER_SOURCE_FILES})
foreach(BOOST_BEAST_FUZZER_SOURCE_FILE ${BOOST_BEAST_FUZZER_SOURCE_FILES})
    file(RELATIVE_PATH BOOST_BEAST_FUZZER_SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR} ${BOOST_BEAST_FUZZER_SOURCE_FILE})
    string(REGEX REPLACE "(.*).cpp" "\\1" RULE_NAME ${BOOST_BEAST_FUZZER_SOURCE_FILE})
    add_boost_beast_fuzzer(${RULE_NAME} ${BOOST_BEAST_FUZZER_SOURCE_FILE})
endforeach()
