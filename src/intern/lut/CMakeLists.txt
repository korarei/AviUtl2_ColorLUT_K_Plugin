cmake_minimum_required(VERSION 3.20.0)

project(LUT LANGUAGES CXX)

# Library definition.
add_library(LUT STATIC
    intern/lut.cpp
)

# Include directories.
target_include_directories(LUT PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Library Link.
target_link_libraries(LUT PUBLIC
    Pixel
    WIC
)

# C++ version specification.
target_compile_features(LUT PUBLIC cxx_std_23)
