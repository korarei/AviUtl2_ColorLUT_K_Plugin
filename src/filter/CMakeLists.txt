cmake_minimum_required(VERSION 3.20.0)

project(Filter LANGUAGES CXX)

# Library definition.
add_library(Filter STATIC
    intern/filter.cpp
    intern/map.cpp
    intern/identity.cpp
)

# Include directories.
target_include_directories(Filter PUBLIC
    "${AVIUTL2_SDK}"
    "${INTERN}"
)

# C++ version specification.
target_compile_features(Filter PUBLIC cxx_std_23)
