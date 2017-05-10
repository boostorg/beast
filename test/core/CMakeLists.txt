# Part of Beast

GroupSources(extras/beast extras)
GroupSources(include/beast beast)
GroupSources(test/core "/")

add_executable (core-tests
    ${BEAST_INCLUDES}
    ${EXTRAS_INCLUDES}
    ../../extras/beast/unit_test/main.cpp
    buffer_test.hpp
    async_result.cpp
    bind_handler.cpp
    buffer_cat.cpp
    buffers_adapter.cpp
    clamp.cpp
    consuming_buffers.cpp
    buffered_read_stream.cpp
    error.cpp
    flat_buffer.cpp
    handler_alloc.cpp
    handler_ptr.cpp
    multi_buffer.cpp
    ostream.cpp
    prepare_buffer.cpp
    static_buffer.cpp
    static_string.cpp
    string_view.cpp
    type_traits.cpp
    base64.cpp
    empty_base_optimization.cpp
    sha1.cpp
)

if (NOT WIN32)
    target_link_libraries(core-tests ${Boost_LIBRARIES} Threads::Threads)
endif()
