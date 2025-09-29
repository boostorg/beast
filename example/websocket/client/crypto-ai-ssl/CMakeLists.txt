#
# Copyright (c) 2025 Mungo Gill
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_executable(websocket-client-crypto-ai-ssl
    Jamfile
    historic_price_fetcher.cpp
    live_price_listener.cpp
    websocket_client_crypto_ai_ssl.cpp)

source_group("" FILES
    Jamfile
    historic_price_fetcher.cpp
    live_price_listener.cpp
    websocket_client_crypto_ai_ssl.cpp)

target_include_directories(websocket-client-crypto-ai-ssl
    PRIVATE ${PROJECT_SOURCE_DIR})

target_link_libraries(websocket-client-crypto-ai-ssl
    PRIVATE Boost::beast Boost::json Boost::url OpenSSL::SSL OpenSSL::Crypto)

set_target_properties(websocket-client-crypto-ai-ssl
    PROPERTIES FOLDER "example-websocket-client")
