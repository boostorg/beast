# Part of Beast

GroupSources(extras/beast extras)
GroupSources(include/beast beast)
GroupSources(test/zlib "/")

if (MSVC)
    set_source_files_properties (${ZLIB_SOURCES} PROPERTIES COMPILE_FLAGS "/wd4127 /wd4131 /wd4244")
endif()

add_executable (zlib-tests
    ${BEAST_INCLUDES}
    ${EXTRAS_INCLUDES}
    ${ZLIB_SOURCES}
    ../../extras/beast/unit_test/main.cpp
    ztest.hpp
    deflate_stream.cpp
    error.cpp
    inflate_stream.cpp
)

target_link_libraries(zlib-tests Beast Boost::program_options Boost::filesystem)
