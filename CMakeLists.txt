# CMakeLists.txt for MLX90640 Thermal Imaging Driver
# Supports PlatformIO and standard CMake builds

cmake_minimum_required(VERSION 3.15)
project(MLX90640_Driver)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Driver source files
set(DRIVER_SOURCES
    MLX90640Driver.cpp
)

set(DRIVER_HEADERS
    MLX90640Driver.h
)

# Create library
add_library(MLX90640Driver STATIC ${DRIVER_SOURCES})

# Include directories
target_include_directories(MLX90640Driver PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Compiler flags for embedded systems
if(MSVC)
    target_compile_options(MLX90640Driver PRIVATE /W4)
else()
    target_compile_options(MLX90640Driver PRIVATE 
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -ffunction-sections
        -fdata-sections
    )
endif()

# Example executable (optional)
add_executable(thermal_imaging_example thermal_imaging_example.cpp)
target_link_libraries(thermal_imaging_example PRIVATE MLX90640Driver)

# Set output directories
set_target_properties(MLX90640Driver PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

set_target_properties(thermal_imaging_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
