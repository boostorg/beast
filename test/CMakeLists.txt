# Part of Beast

GroupSources(extras/beast extras)
GroupSources(include/beast beast)
GroupSources(test "/")

add_executable (lib-tests
    ${BEAST_INCLUDES}
    ${EXTRAS_INCLUDES}
    ../extras/beast/unit_test/main.cpp
    config.cpp
    core.cpp
    http.cpp
    version.cpp
    websocket.cpp
    zlib.cpp
)

target_link_libraries(lib-tests Beast ${Boost_PROGRAM_OPTIONS_LIBRARY})

if (MINGW)
    set_target_properties(lib-tests PROPERTIES COMPILE_FLAGS "-Wa,-mbig-obj")
endif()
