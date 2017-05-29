# Part of Beast

GroupSources(extras/beast extras)
GroupSources(include/beast beast)
GroupSources(test/http "/")

add_executable (http-tests
    ${BEAST_INCLUDES}
    ${EXTRAS_INCLUDES}
    message_fuzz.hpp
    test_parser.hpp
    ../../extras/beast/unit_test/main.cpp
    basic_parser.cpp
    buffer_body.cpp
    design.cpp
    dynamic_body.cpp
    empty_body.cpp
    error.cpp
    fields.cpp
    header_parser.cpp
    message.cpp
    message_parser.cpp
    read.cpp
    rfc7230.cpp
    serializer.cpp
    string_body.cpp
    type_traits.cpp
    write.cpp
)

if (NOT WIN32)
    target_link_libraries(http-tests ${Boost_LIBRARIES} Threads::Threads)
else()
    target_link_libraries(http-tests ${Boost_LIBRARIES})
endif()

add_executable (http-bench
    ${BEAST_INCLUDES}
    ${EXTRAS_INCLUDES}
    nodejs_parser.hpp
    ../../extras/beast/unit_test/main.cpp
    nodejs_parser.cpp
    parser_bench.cpp
)

if (NOT WIN32)
    target_link_libraries(http-bench ${Boost_LIBRARIES} Threads::Threads)
else()
    target_link_libraries(http-bench ${Boost_LIBRARIES})
endif()
