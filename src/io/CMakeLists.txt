cmake_minimum_required(VERSION 3.20.0)

project(IO LANGUAGES CXX)

# Library definition.
add_library(IO STATIC
    intern/io.cpp
    intern/importer.cpp
    intern/exporter.cpp
)

# Include directories.
target_include_directories(IO PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${AVIUTL2_SDK}"
    "${INTERN}"
)

# C++ version specification.
target_compile_features(IO PUBLIC cxx_std_23)
