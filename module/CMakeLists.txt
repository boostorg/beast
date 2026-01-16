cmake_minimum_required(VERSION 3.28)

add_library(boost_beast_module)

target_sources(boost_beast_module
    PUBLIC
        FILE_SET CXX_MODULES FILES
            boost.cppm
)

target_compile_features(boost_beast_module PUBLIC cxx_std_20)

target_include_directories(boost_beast_module PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
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
