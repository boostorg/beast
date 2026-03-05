cmake_minimum_required(VERSION 3.28)

add_library(boost_beast_module)

target_sources(boost_beast_module
    PUBLIC
        FILE_SET CXX_MODULES FILES
            beast.cppm
)

target_compile_features(boost_beast_module PUBLIC cxx_std_20)

target_include_directories(boost_beast_module PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/config/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/asio/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/assert/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/bind/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/container/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/container_hash/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/core/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/describe/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/endian/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/intrusive/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/io/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/logic/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/move/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/mp11/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/optional/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/smart_ptr/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/static_string/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/system/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/throw_exception/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/type_index/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/type_traits/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/utility/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../boost/winapi/include>
    $<INSTALL_INTERFACE:include>
)

add_library(Boost::beast::module ALIAS boost_beast_module)

# Installation
install(TARGETS boost_beast_module
    EXPORT ${PROJECT_NAME}Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    FILE_SET CXX_MODULES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/module
)
