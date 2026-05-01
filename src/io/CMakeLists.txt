cmake_minimum_required(VERSION 3.20.0)

project(IO LANGUAGES CXX)

# Library definition.
add_library(${PROJECT_NAME} STATIC
    intern/io.cpp
    intern/importer.cpp
    intern/exporter.cpp
)

# Include directories.
target_include_directories(${PROJECT_NAME}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Library Link.
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        LUT
        Utilities
        AviUtl2SDK
)

# C++ version specification.
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
