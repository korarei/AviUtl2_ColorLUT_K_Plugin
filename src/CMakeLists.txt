cmake_minimum_required(VERSION 3.20.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Properties.
set(VERSION "0.1.0" CACHE STRING "Project version")

project(ColorLUT_K VERSION ${VERSION} LANGUAGES CXX)

if (NOT WIN32)
    message(FATAL_ERROR "This project is only for Windows")
endif()

if (NOT CMAKE_SIZEOF_VOID_P MATCHES 8)
    message(FATAL_ERROR "This project is only for x64")
endif()

if (NOT CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|amd64|AMD64)")
    message(FATAL_ERROR "This project is only for x64")
endif()

# Path settings.
set(CMAKE_SCRIPTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(AVIUTL2_SDK "${CMAKE_CURRENT_SOURCE_DIR}/extern/aviutl2_sdk/include/aviutl2_sdk")
set(INCLUDES
    "${AVIUTL2_SDK}"
    "${CMAKE_CURRENT_SOURCE_DIR}/intern"
    "${CMAKE_CURRENT_SOURCE_DIR}/intern/pixel"
    "${CMAKE_CURRENT_SOURCE_DIR}/intern/wic"
    "${CMAKE_CURRENT_SOURCE_DIR}/intern/lut"
    "${CMAKE_CURRENT_SOURCE_DIR}/intern/direct3d"
)

# Compiler Dependent Options.
if (MSVC)
    # Runtime Linkage.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    add_compile_options(
        /utf-8
        /arch:AVX2
        /W4 # Warning Level 4.
        /permissive- # Standards-based mode.

        # Release-only optimizations.
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/GL> # Link Optimization.
        $<$<CONFIG:Release>:/Gy> # Split by function. (Remove unused functions.)

        # Debug settings.
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Debug>:/Zi> # Debugging Information.
    )

    add_link_options(
        $<$<CONFIG:Release>:/LTCG> # Runtime Optimization.
        $<$<CONFIG:Debug>:/DEBUG> # Link with debug information.
    )
else()
    message(FATAL_ERROR "This project is only for MSVC")
endif()

add_compile_definitions(
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    VERSION=L"${VERSION}"
)

# Subdirectories.
add_subdirectory(intern/pixel)
add_subdirectory(intern/wic)
add_subdirectory(intern/lut)
add_subdirectory(intern/direct3d)
add_subdirectory(filter)
add_subdirectory(io)

# Main target definition.
add_library(${PROJECT_NAME} SHARED
    main.def
    main.cpp
)

# Include directories.
target_include_directories(${PROJECT_NAME} PRIVATE
    "${AVIUTL2_SDK}"
)

# Library Link.
target_link_libraries(${PROJECT_NAME} PRIVATE
    Pixel
    WIC
    LUT
    Direct3D
    Filter
    IO
)

# Output settings.
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    SUFFIX ".aux2"
)

# C++ version specification.
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
