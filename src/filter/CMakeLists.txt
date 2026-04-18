cmake_minimum_required(VERSION 3.20.0)

project(Filter LANGUAGES CXX)

# Library definition.
add_library(Filter STATIC
    intern/filter.cpp
    intern/transform.cpp
    intern/identity.cpp
)

# Shader compilation.
include(${CMAKE_SCRIPTS}/CompileShaders.cmake)
set(SHADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/intern/shaders/lut1d.hlsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/intern/shaders/lut3d.hlsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/intern/shaders/identity.hlsl"
)
compile_pixel_shaders(Filter "${SHADERS}")

# Include directories.
target_include_directories(Filter PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    ${INCLUDES}
)

target_link_libraries(Filter PRIVATE Direct3D)

# C++ version specification.
target_compile_features(Filter PUBLIC cxx_std_23)
