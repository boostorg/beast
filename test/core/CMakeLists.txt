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
    buffer_concepts.cpp
    buffers_adapter.cpp
    clamp.cpp
    consuming_buffers.cpp
    buffered_read_stream.cpp
    error.cpp
    flat_buffer.cpp
    handler_alloc.cpp
    handler_concepts.cpp
    handler_ptr.cpp
    multi_buffer.cpp
    ostream.cpp
    prepare_buffer.cpp
    prepare_buffers.cpp
    static_buffer.cpp
    static_string.cpp
    stream_concepts.cpp
    string_view.cpp
    base64.cpp
    empty_base_optimization.cpp
    sha1.cpp
    type_traits.cpp
)

if (NOT WIN32)
    target_link_libraries(core-tests ${Boost_LIBRARIES} Threads::Threads)
else()
    target_link_libraries(core-tests ${Boost_LIBRARIES})
endif()
