# Include common options.
include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)

# Static runtime linkage.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "")

# Compiler options.
# Specific to C and CXX, to prevent passing them to MASM
add_compile_options(
    $<$<COMPILE_LANGUAGE:CXX>:/bigobj>
    $<$<COMPILE_LANGUAGE:CXX>:/permissive->
    $<$<COMPILE_LANGUAGE:C,CXX>:/W4>
    $<$<COMPILE_LANGUAGE:C,CXX>:/MP>
)
if("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32") # 32-bit
    add_compile_options(
        /arch:SSE2
    )
endif()

# Linker options.
add_link_options(
)

# Disable logos.
foreach(lang C CXX ASM_MASM RC)
    set(CMAKE_${lang}_FLAGS_INIT "/nologo")
endforeach()
foreach(type EXE SHARED MODULE)
    set(CMAKE_${type}_LINKER_FLAGS_INIT "/nologo")
endforeach()

# Silence Visual Studio CMake integration warnings.
set(SILENCE_VS_DEFINITIONS ${CMAKE_TOOLCHAIN_FILE} ${CMAKE_C_COMPILER})
set(SILENCE_VS_DEFINITIONS)
