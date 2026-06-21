cmake_minimum_required(VERSION 3.20.0)

project(Filter LANGUAGES CXX)

# Library definition.
add_library(${PROJECT_NAME} STATIC
    intern/filter.cpp
    intern/grading.cpp
    intern/neutral.cpp
)

# Shader compilation.
include(${CMAKE_SCRIPTS}/CompileShaders.cmake)
set(SHADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/intern/shaders/lut1d.hlsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/intern/shaders/lut3d.hlsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/intern/shaders/hald.hlsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/intern/shaders/strip.hlsl"
)
compile_pixel_shaders(${PROJECT_NAME} "${SHADERS}")

# Include directories.
target_include_directories(${PROJECT_NAME}
    PUBLIC
        "${CMAKE_SOURCE_DIR}"
)

# Link libraries.
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        AviUtl2SDK
        Direct3D
        LUT
)

# C++ version specification.
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
