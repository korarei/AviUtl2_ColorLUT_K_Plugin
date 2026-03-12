cmake_minimum_required(VERSION 3.20.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(VERSION "0.1.0" CACHE STRING "Project version")
project(ColorLUT_K VERSION ${VERSION} LANGUAGES CXX)

# Path settings.
set(AVIUTL2_SDK "${CMAKE_SOURCE_DIR}/external/aviutl2_sdk/include/aviutl2_sdk")
file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.hlsl")

# Include scripts.
include(cmake/CompileShaders.cmake)

# Main target definition.
add_library(${PROJECT_NAME} SHARED
    main.def
    main.cpp
    filter.cpp
    identity.cpp
    exporter.cpp
    # importer.cpp
    core/blend.cpp
    core/lut.cpp
)

# Shader compilation.
compile_shaders(${PROJECT_NAME} "${SHADER_SOURCES}")

# Include directories.
target_include_directories(${PROJECT_NAME} PRIVATE
    "${AVIUTL2_SDK}"
)

# Library Link.
target_link_libraries(${PROJECT_NAME} PRIVATE
    d2d1
    d3d11
    dxgi
    dxguid
)

# Output settings.
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    SUFFIX ".aux2"
)

# C++ version specification.
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)

# Definitions.
target_compile_definitions(${PROJECT_NAME} PRIVATE
    VERSION=L"${VERSION}"
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

# Compiler Dependent Options.
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
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

    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:/LTCG> # Runtime Optimization.
        $<$<CONFIG:Debug>:/DEBUG> # Link with debug information.
    )

    # Runtime library switching.
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()
