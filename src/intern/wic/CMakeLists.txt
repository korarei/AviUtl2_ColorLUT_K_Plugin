cmake_minimum_required(VERSION 3.20.0)

project(WIC LANGUAGES CXX)

# Library definition.
add_library(WIC STATIC
    intern/wic.cpp
)

# Include directories.
target_include_directories(WIC PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# C++ version specification.
target_compile_features(WIC PUBLIC cxx_std_23)
