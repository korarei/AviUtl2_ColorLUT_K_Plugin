cmake_minimum_required(VERSION 3.20.0)

project(Direct3D LANGUAGES CXX)

add_library(${PROJECT_NAME} OBJECT
    intern/direct3d.cpp
)

include(${CMAKE_SCRIPTS}/CompileShaders.cmake)
set(VERTEX_SHADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/intern/shaders/fullscreen.hlsl"
)
compile_vertex_shaders(${PROJECT_NAME} "${VERTEX_SHADERS}")

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        d3d11
        Pixel
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
