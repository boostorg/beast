# Part of Beast

cmake_minimum_required (VERSION 3.2)

project (Beast)

set(BEAST_VERSION_MAJOR "1")
set(BEAST_VERSION_MINOR "0")
set(BEAST_VERSION_PATCH "0")
set(BEAST_VERSION_BUILD "b20")
set(BEAST_VERSION "${BEAST_VERSION_MAJOR}.${BEAST_VERSION_MINOR}.${BEAST_VERSION_PATCH}-${BEAST_VERSION_BUILD}")

option(BEAST_INSTALL "Installation targets."  ON)

set_property (GLOBAL PROPERTY USE_FOLDERS ON)

if (MSVC)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4244 /MP /W4 /wd4100 /bigobj /D _WIN32_WINNT=0x0601 /D _SCL_SECURE_NO_WARNINGS=1 /D _CRT_SECURE_NO_WARNINGS=1")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ob2 /Oi /Ot /GL /MT")
    set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Oi /Ot /MT")

    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
    set (CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")

    # for RelWithDebInfo builds, disable incremental linking
    # since CMake sets it ON by default for that build type and it
    # causes warnings
    string (REPLACE "/INCREMENTAL" "/INCREMENTAL:NO" replacement_flags
        ${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO})
    set (CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO ${replacement_flags})

else()
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_MULTITHREADED ON)
    find_package(Boost REQUIRED COMPONENTS coroutine context thread filesystem program_options system)
    include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIR})

    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads)

    set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Wpedantic -Wno-unused-parameter")
endif()

if (APPLE AND NOT DEFINED ENV{OPENSSL_ROOT_DIR})
  find_program(HOMEBREW brew)
  if (NOT HOMEBREW STREQUAL "HOMEBREW-NOTFOUND")
    execute_process(COMMAND brew --prefix openssl
      OUTPUT_VARIABLE OPENSSL_ROOT_DIR
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()
endif()

find_package(OpenSSL)

if (MINGW)
    link_libraries(${Boost_LIBRARIES} ws2_32 mswsock)
endif()

if ("${VARIANT}" STREQUAL "coverage")
    set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CMAKE_BUILD_TYPE RELWITHDEBINFO)
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lgcov")
elseif ("${VARIANT}" STREQUAL "asan")
    set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_BUILD_TYPE RELWITHDEBINFO)
elseif ("${VARIANT}" STREQUAL "usan")
    set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fno-omit-frame-pointer")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
    set(CMAKE_BUILD_TYPE RELWITHDEBINFO)
elseif ("${VARIANT}" STREQUAL "debug")
    set(CMAKE_BUILD_TYPE DEBUG)
elseif ("${VARIANT}" STREQUAL "release")
    set(CMAKE_BUILD_TYPE RELEASE)
endif()

function(DoGroupSources curdir rootdir folder)
    file(GLOB children RELATIVE ${PROJECT_SOURCE_DIR}/${curdir} ${PROJECT_SOURCE_DIR}/${curdir}/*)
    foreach(child ${children})
        if(IS_DIRECTORY ${PROJECT_SOURCE_DIR}/${curdir}/${child})
            DoGroupSources(${curdir}/${child} ${rootdir} ${folder})
        elseif(${child} STREQUAL "CMakeLists.txt")
            source_group("" FILES ${PROJECT_SOURCE_DIR}/${curdir}/${child})
        else()
            string(REGEX REPLACE ^${rootdir} ${folder} groupname ${curdir})
            string(REPLACE "/" "\\" groupname ${groupname})
            source_group(${groupname} FILES ${PROJECT_SOURCE_DIR}/${curdir}/${child})
        endif()
    endforeach()
endfunction()

function(GroupSources curdir folder)
    DoGroupSources(${curdir} ${curdir} ${folder})
endfunction()

include_directories (extras)
include_directories (include)

file(GLOB_RECURSE BEAST_INCLUDES
    ${PROJECT_SOURCE_DIR}/include/beast/*.hpp
    ${PROJECT_SOURCE_DIR}/include/beast/*.ipp
)

file(GLOB_RECURSE EXTRAS_INCLUDES
    ${PROJECT_SOURCE_DIR}/extras/beast/*.hpp
    ${PROJECT_SOURCE_DIR}/extras/beast/*.ipp
)

add_subdirectory (examples)
if (NOT OPENSSL_FOUND)
    message("OpenSSL not found. Not building examples/ssl")
else()
    add_subdirectory (examples/ssl)
endif()

add_subdirectory (test)
add_subdirectory (test/core)
add_subdirectory (test/http)
add_subdirectory (test/websocket)
add_subdirectory (test/zlib)


if (BEAST_INSTALL AND (UNIX OR CYGWIN))

  add_library(BeastLibrary INTERFACE)
  target_include_directories(BeastLibrary INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)

  include(CMakePackageConfigHelpers)

  set(INCLUDE_INSTALL_DIR include CACHE STRING "Installation directory for headers, relative to ${CMAKE_INSTALL_PREFIX}.")
  set(LIB_INSTALL_DIR lib CACHE STRING "Installation directory for libraries, relative to ${CMAKE_INSTALL_PREFIX}.")
  set(CMAKE_INSTALL_DIR lib/cmake/Beast CACHE STRING "Installation directory for cmake files, relative to ${CMAKE_INSTALL_PREFIX}.")
  set(DOC_INSTALL_DIR share/doc/Beast CACHE PATH "Installation directory for documentation, relative to ${CMAKE_INSTALL_PREFIX}.")

  set(VERSION_CONFIG ${PROJECT_BINARY_DIR}/BeastConfigVersion.cmake)
  set(PROJECT_CONFIG ${PROJECT_BINARY_DIR}/BeastConfig.cmake)
  set(TARGETS_EXPORT BeastTargets)
  set(INSTALL_TARGETS BeastLibrary)

  write_basic_package_version_file(${VERSION_CONFIG} VERSION ${BEAST_VERSION} COMPATIBILITY AnyNewerVersion)

  configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/BeastConfig.cmake.in
    ${PROJECT_CONFIG}
    INSTALL_DESTINATION ${CMAKE_INSTALL_DIR})

  # pkg-config
  if (UNIX OR CYGWIN)
    configure_file(${PROJECT_SOURCE_DIR}/Beast.pc.in ${CMAKE_CURRENT_BINARY_DIR}/Beast.pc @ONLY)
  endif()

  export(TARGETS ${INSTALL_TARGETS} FILE ${PROJECT_BINARY_DIR}/${TARGETS_EXPORT}.cmake)

  install(FILES ${PROJECT_CONFIG} ${VERSION_CONFIG} DESTINATION ${CMAKE_INSTALL_DIR})
  install(EXPORT ${TARGETS_EXPORT} DESTINATION ${CMAKE_INSTALL_DIR})
  install(TARGETS ${INSTALL_TARGETS} EXPORT ${TARGETS_EXPORT} DESTINATION ${LIB_INSTALL_DIR})
  install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION ${INCLUDE_INSTALL_DIR})
  install(FILES ${PROJECT_SOURCE_DIR}/LICENSE_1_0.txt ${PROJECT_SOURCE_DIR}/README.md DESTINATION ${DOC_INSTALL_DIR})

  # pkg-config
  if (UNIX OR CYGWIN)
    install(FILES ${PROJECT_BINARY_DIR}/Beast.pc DESTINATION "${LIB_INSTALL_DIR}/pkgconfig")
  endif()

  include(InstallRequiredSystemLibraries)
  set(CPACK_PACKAGE_NAME "libbeast-dev")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "HTTP and WebSocket implementations built on Boost.Asio in C++11.")
  set(CPACK_PACKAGE_VERSION "${BEAST_VERSION}")
  set(CPACK_PACKAGE_VERSION_MAJOR "${BEAST_VERSION_MAJOR}")
  set(CPACK_PACKAGE_VERSION_MINOR "${BEAST_VERSION_MINOR}")
  set(CPACK_PACKAGE_VERSION_PATCH "${BEAST_VERSION_PATCH}")
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE_1_0.txt")
  set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
  set(CPACK_SOURCE_IGNORE_FILES "/\\\\.git/;/\\\\.gitignore;/CMakeCache.txt")
  set(CPACK_SYSTEM_NAME "all")
  set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
  set(CPACK_PACKAGE_FILE_NAME "libbeast-dev-${BEAST_VERSION}-${CPACK_SYSTEM_NAME}")
  set(CPACK_GENERATOR "DEB")
  set(CPACK_SOURCE_GENERATOR "DEB")
  set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/vinniefalco/Beast")
  set(CPACK_PACKAGE_VENDOR "Beast")
  set(CPACK_PACKAGE_CONTACT "aleksandar.valchev@gmail.com")

  set(CPACK_DEBIAN_ARCHITECTURE all)
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Aleksandar Valchev")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libc6 (>= 2.3) , libssl-dev (>=1.0.2) , libboost-coroutine-dev , libboost-context-dev , libboost-thread-dev , libboost-filesystem-dev , libboost-program-options-dev "
    )
  set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
  set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
  set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/vinniefalco/Beast")
  include(CPack)

endif()
