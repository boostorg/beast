# Part of Beast

GroupSources(examples examples)
GroupSources(extras/beast extras)
GroupSources(include/beast beast)
GroupSources(test/http "/")

add_executable (http-tests
    ${BEAST_INCLUDES}
    ${EXAMPLES_INCLUDES}
    ${EXTRAS_INCLUDES}
    message_fuzz.hpp
    test_parser.hpp
    ../../extras/beast/unit_test/main.cpp
    basic_parser.cpp
    buffer_body.cpp
    doc_http_samples.cpp
    doc_snippets.cpp
    dynamic_body.cpp
    empty_body.cpp
    error.cpp
    field.cpp
    fields.cpp
    message.cpp
    parser.cpp
    read.cpp
    rfc7230.cpp
    serializer.cpp
    status.cpp
    string_body.cpp
    type_traits.cpp
    verb.cpp
    write.cpp
)

target_link_libraries(http-tests Beast Boost::program_options Boost::filesystem Boost::coroutine Boost::thread Boost::context)

add_executable (http-bench
    ${BEAST_INCLUDES}
    ${EXTRAS_INCLUDES}
    nodejs_parser.hpp
    message_fuzz.hpp
    ../../extras/beast/unit_test/main.cpp
    nodejs_parser.cpp
    parser_bench.cpp
)

target_link_libraries(http-bench Beast Boost::program_options Boost::filesystem)
