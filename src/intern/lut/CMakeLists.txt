cmake_minimum_required(VERSION 3.20.0)

project(LUT LANGUAGES CXX)

add_library(${PROJECT_NAME} OBJECT
    intern/lut.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Pixel
    PRIVATE
        WIC
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
