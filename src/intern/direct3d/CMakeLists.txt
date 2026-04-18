cmake_minimum_required(VERSION 3.20.0)

project(Direct3D LANGUAGES CXX)

# Library definition.
add_library(Direct3D STATIC
    intern/direct3d.cpp
)

# Shader compilation.
include(${CMAKE_SCRIPTS}/CompileShaders.cmake)
set(SHADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/intern/shaders/fullscreen.hlsl"
)
compile_vertex_shaders(Direct3D "${SHADERS}")

# Include directories.
target_include_directories(Direct3D PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Link libraries.
target_link_libraries(Direct3D PUBLIC
    d3d11
    dxgi
    dxguid
)

# C++ version specification.
target_compile_features(Direct3D PUBLIC cxx_std_23)
