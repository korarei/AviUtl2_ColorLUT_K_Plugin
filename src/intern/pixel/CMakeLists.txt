cmake_minimum_required(VERSION 3.20.0)

project(Pixel LANGUAGES CXX)

find_package(Eigen3 CONFIG REQUIRED)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME}
    INTERFACE
        "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(${PROJECT_NAME}
    INTERFACE
        Eigen3::Eigen
)

target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_23)
