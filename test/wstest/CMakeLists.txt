# Part of Beast

GroupSources(include/beast beast)
GroupSources(example/common common)
GroupSources(extras/beast extras)
GroupSources(test/wstest "/")

add_executable (wstest
    ${BEAST_INCLUDES}
    ${COMMON_INCLUDES}
    ${EXTRAS_INCLUDES}
    main.cpp
    )

target_link_libraries(wstest
    Beast
    )
