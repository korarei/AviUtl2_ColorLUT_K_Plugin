cmake_minimum_required(VERSION 3.20.0)

project(LUT LANGUAGES CXX)

# Library definition.
add_library(${PROJECT_NAME} STATIC
    intern/lut.cpp
)

# Include directories.
target_include_directories(${PROJECT_NAME}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Library Link.
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Pixel
    PRIVATE
        WIC
)

# C++ version specification.
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
