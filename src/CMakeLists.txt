cmake_minimum_required(VERSION 3.20.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(VERSION "0.1.0" CACHE STRING "Project version")
project(ColorLUT_K VERSION ${VERSION} LANGUAGES CXX)

# Path settings.
set(AVIUTL2_SDK "${CMAKE_SOURCE_DIR}/external/aviutl2_sdk/include/aviutl2_sdk")
set(SHADER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_DST "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SHADER_DST}")

# Find fxc.exe for shader compilation.
find_program(FXC_EXECUTABLE fxc)

if(NOT FXC_EXECUTABLE)
    message(FATAL_ERROR "fxc.exe not found.")
endif()

# Compute shaders (cs_5_0).
file(GLOB CS_SHADERS CONFIGURE_DEPENDS "${SHADER_SRC}/*.cs.hlsl")

set(SHADER_HEADERS "")
foreach(SHADER IN LISTS CS_SHADERS)
    get_filename_component(FILE_NAME "${SHADER}" NAME_WE)
    set(SHADER_HEADER "${SHADER_DST}/${FILE_NAME}.cs.h")

    add_custom_command(
        OUTPUT "${SHADER_HEADER}"
        COMMAND "${FXC_EXECUTABLE}"
                /T cs_5_0
                /E main
                /O3
                /WX
                /Qstrip_reflect
                /Qstrip_debug
                /Fh "${SHADER_HEADER}"
                /Vn "g_${FILE_NAME}_cs"
                "${SHADER}"
        DEPENDS "${SHADER}"
        VERBATIM
    )

    list(APPEND SHADER_HEADERS "${SHADER_HEADER}")
endforeach()

# Pixel shaders (ps_5_0).
file(GLOB PS_SHADERS CONFIGURE_DEPENDS "${SHADER_SRC}/*.ps.hlsl")

foreach(SHADER IN LISTS PS_SHADERS)
    get_filename_component(FILE_NAME "${SHADER}" NAME_WE)
    set(SHADER_HEADER "${SHADER_DST}/${FILE_NAME}.ps.h")

    add_custom_command(
        OUTPUT "${SHADER_HEADER}"
        COMMAND "${FXC_EXECUTABLE}"
                /T ps_5_0
                /E main
                /O3
                /WX
                /Qstrip_reflect
                /Qstrip_debug
                /Fh "${SHADER_HEADER}"
                /Vn "g_${FILE_NAME}_ps"
                "${SHADER}"
        DEPENDS "${SHADER}"
        VERBATIM
    )

    list(APPEND SHADER_HEADERS "${SHADER_HEADER}")
endforeach()

# Create a custom target for shaders.
add_custom_target(Shaders DEPENDS ${SHADER_HEADERS})

# Main target definition.
add_library(${PROJECT_NAME} SHARED
    main.def
    main.cpp
    blend.cpp
    lut.cpp
    filter.cpp
    identity.cpp
    exporter.cpp
    # importer.cpp
)

# Ensure shaders are built before the main target.
add_dependencies(${PROJECT_NAME} Shaders)

# Include directories.
target_include_directories(${PROJECT_NAME} PRIVATE
    "${AVIUTL2_SDK}"
    "${SHADER_DST}"
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
