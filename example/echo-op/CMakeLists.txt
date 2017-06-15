# Part of Beast

GroupSources(extras/beast extras)
GroupSources(include/beast beast)

GroupSources(example/echo-op "/")

add_executable (echo-op
    ${BEAST_INCLUDES}
    echo_op.cpp
)

target_link_libraries(echo-op Beast)
