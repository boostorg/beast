# Part of Beast

GroupSources(examples examples)
GroupSources(extras/beast extras)
GroupSources(include/beast beast)
GroupSources(test/core "/")

add_executable (core-tests
    ${BEAST_INCLUDES}
    ${EXAMPLES_INCLUDES}
    ${EXTRAS_INCLUDES}
    ../../extras/beast/unit_test/main.cpp
    async_result.cpp
    bind_handler.cpp
    buffer_bench.cpp
    buffer_cat.cpp
    buffer_prefix.cpp
    buffer_test.hpp
    buffered_read_stream.cpp
    buffers_adapter.cpp
    clamp.cpp
    consuming_buffers.cpp
    doc_core_samples.cpp
    doc_snippets.cpp
    drain_buffer.cpp
    error.cpp
    flat_buffer.cpp
    handler_alloc.cpp
    handler_ptr.cpp
    multi_buffer.cpp
    ostream.cpp
    static_buffer.cpp
    static_string.cpp
    string_param.cpp
    string_view.cpp
    type_traits.cpp
    base64.cpp
    empty_base_optimization.cpp
    sha1.cpp
)

target_link_libraries(core-tests Beast Boost::program_options Boost::filesystem Boost::coroutine Boost::thread Boost::context)
