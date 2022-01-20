#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

GroupSources(include/boost/beast beast)
GroupSources(example/http/client/sync "/")

add_executable (http-client-sync-get
    ${BOOST_BEAST_FILES}
    Jamfile
    http_client_sync_get.cpp
)

target_link_libraries(http-client-sync-get
    lib-asio
    lib-beast)

set_property(TARGET http-client-sync-get PROPERTY FOLDER "example-http-client")

add_executable (http-client-sync-post
    ${BOOST_BEAST_FILES}
    Jamfile
    http_client_sync_post.cpp
)

target_link_libraries(http-client-sync-post
    lib-asio
    lib-beast)

set_property(TARGET http-client-sync-post PROPERTY FOLDER "example-http-client")

add_executable (http-client-sync-json
    ${BOOST_BEAST_FILES}
    Jamfile
    http_client_sync_json.cpp
)

target_link_libraries(http-client-sync-json
    lib-asio
    lib-beast)

set_property(TARGET http-client-sync-json PROPERTY FOLDER "example-http-client")

add_executable (http-client-sync-file-upload
    ${BOOST_BEAST_FILES}
    Jamfile
    http_client_sync_file_upload.cpp
)

target_link_libraries(http-client-sync-file-upload
    lib-asio
    lib-beast)

set_property(TARGET http-client-sync-file-upload PROPERTY FOLDER "example-http-client")
