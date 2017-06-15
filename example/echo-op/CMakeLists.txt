# Part of Beast

GroupSources(extras/beast extras)
GroupSources(include/beast beast)

GroupSources(example/echo-op "/")

add_executable (echo-op
    ${BEAST_INCLUDES}
    echo_op.cpp
)

if (NOT WIN32)
    target_link_libraries(echo-op ${Boost_LIBRARIES} Threads::Threads)
else()
    target_link_libraries(echo-op ${Boost_LIBRARIES})
endif()
