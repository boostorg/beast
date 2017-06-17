# Part of Beast

GroupSources(include/beast beast)

GroupSources(example/server-framework "/")

if (OPENSSL_FOUND)
  include_directories(${OPENSSL_INCLUDE_DIR})
endif()

add_executable (server-framework
    ${BEAST_INCLUDES}
    ${SERVER_INCLUDES}
    main.cpp
)

target_link_libraries(
    server-framework
    Beast
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY})

if (OPENSSL_FOUND)
  target_link_libraries(server-framework ${OPENSSL_LIBRARIES})
endif()
