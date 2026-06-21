cmake_minimum_required(VERSION 3.20.0)

project(Pixel LANGUAGES CXX)

find_package(Eigen3 CONFIG REQUIRED)

add_library(${PROJECT_NAME} OBJECT
    intern/pixel.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Eigen3::Eigen
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
