cmake_minimum_required(VERSION 3.20.0)

set(VERSION "0.1.0" CACHE STRING "Project version")
project(ColorLUT_K VERSION ${VERSION} LANGUAGES CXX)

# Path settings.
set(AVIUTL2_SDK "${CMAKE_SOURCE_DIR}/external/aviutl2_sdk/include/aviutl2_sdk")
set(SHADER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_DST "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(GLOB HLSL_SOURCES CONFIGURE_DEPENDS "${SHADER_SRC}/*.hlsl")
file(MAKE_DIRECTORY "${SHADER_DST}")

# Find fxc.exe for shader compilation.
find_program(FXC_EXECUTABLE fxc
    HINTS "$ENV{WindowsSdkDir}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64/"
)

if(NOT FXC_EXECUTABLE)
    message(FATAL_ERROR "fxc.exe not found.")
endif()

# Compile all HLSL files.
set(SHADER_HEADERS "")
foreach(HLSL IN LISTS HLSL_SOURCES)
    get_filename_component(FILE_NAME "${HLSL}" NAME_WE)
    set(HEADER "${SHADER_DST}/${FILE_NAME}.h")

    add_custom_command(
        OUTPUT "${HEADER}"
        COMMAND "${FXC_EXECUTABLE}"
                /T cs_5_0
                /E main
                /Fh "${HEADER}"
                /Vn "g_${FILE_NAME}"
                "${HLSL}"
        DEPENDS "${HLSL}"
        VERBATIM
    )

    list(APPEND SHADER_HEADERS "${HEADER}")
endforeach()

# Create a custom target for shaders.
add_custom_target(Shaders DEPENDS ${SHADER_HEADERS})

# Main target definition.
add_library(${PROJECT_NAME} SHARED
    main.def
    main.cpp
    lut.cpp
    filter.cpp
    baker.cpp
)

# Ensure shaders are built before the main target.
add_dependencies(${PROJECT_NAME} Shaders)

# Include directories.
target_include_directories(${PROJECT_NAME} PRIVATE
    "${AVIUTL2_SDK}"
    "${SHADER_DST}"
)

# Library Link.
target_link_libraries(${PROJECT_NAME} PRIVATE
    d2d1
    d3d11
    dxgi
    dxguid
)

# Output settings.
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    SUFFIX ".aux2"
)

# C++ version specification.
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)

# Definitions.
target_compile_definitions(${PROJECT_NAME} PRIVATE
    VERSION=L"${VERSION}"
)

# Compiler Dependent Options.
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /utf-8
        /arch:AVX2
        /W4 # Warning Level 4.
        /permissive- # Standards-based mode.

        # Release-only optimizations.
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/GL> # Link Optimization.
        $<$<CONFIG:Release>:/Gy> # Split by function. (Remove unused functions.)

        # Debug settings.
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Debug>:/Zi> # Debugging Information.
    )

    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:/LTCG> # Runtime Optimization.
        $<$<CONFIG:Debug>:/DEBUG> # Link with debug information.
    )

    # Runtime library switching.
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()
