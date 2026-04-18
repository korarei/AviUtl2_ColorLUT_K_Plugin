cmake_minimum_required(VERSION 3.20.0)

project(Pixel LANGUAGES CXX)

# Library definition.
add_library(Pixel STATIC
    intern/pixel.cpp
)

# Include directories.
target_include_directories(Pixel PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# C++ version specification.
target_compile_features(Pixel PUBLIC cxx_std_23)
