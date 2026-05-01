cmake_minimum_required(VERSION 3.20.0)

project(Direct3D LANGUAGES CXX)

# Library definition.
add_library(${PROJECT_NAME} STATIC
    intern/direct3d.cpp
)

# Shader compilation.
include(${CMAKE_SCRIPTS}/CompileShaders.cmake)
set(VERTEX_SHADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/intern/shaders/fullscreen.hlsl"
)
set(COMPUTE_SHADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/intern/shaders/blit.hlsl"
)
compile_vertex_shaders(${PROJECT_NAME} "${VERTEX_SHADERS}")
compile_compute_shaders(${PROJECT_NAME} "${COMPUTE_SHADERS}")

# Include directories.
target_include_directories(${PROJECT_NAME}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Link libraries.
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        d3d11
        dxgi
        dxguid
        Pixel
)

# C++ version specification.
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
