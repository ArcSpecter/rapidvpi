#MIT License
#
#Copyright (c) 2024 Rovshan Rustamov
#
#Permission is hereby granted, free of charge, to any person obtaining a copy
#of this software and associated documentation files (the "Software"), to deal
#in the Software without restriction, including without limitation the rights
#to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#copies of the Software, and to permit persons to whom the Software is
#furnished to do so, subject to the following conditions:
#
#The above copyright notice and this permission notice shall be included in all
#copies or substantial portions of the Software.
#
#THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#SOFTWARE.

# Description: Main CMakeLists.txt file for the RapidVPI library. Includes build
# and installation steps. The following environment variables must be set for the
# system:
# VPI_INCLUDE_DIR: pointing to the location where the "vpi_user.h" file is located (i.e. for
# the case of iverilog it is "path/to/iverilog")

# Main CMakeLists.txt for the library
cmake_minimum_required(VERSION 3.5)

# Project configuration
project(rapidvpi VERSION 0.0.1)

# ---------------------------------------------------------------------------
# Debug prints control for RapidVPI
# ---------------------------------------------------------------------------
option(RAPIDVPI_ENABLE_DEBUG "Enable RapidVPI internal debug prints" OFF)

if (RAPIDVPI_ENABLE_DEBUG)
    add_definitions(-DRAPIDVPI_DEBUG)
endif ()
# ---------------------------------------------------------------------------

# Include the CMake package config helpers
include(CMakePackageConfigHelpers)

# Set project variables
# NOTE: For Questa, you only need VPI_INCLUDE_DIR
set(vpi_include_dir $ENV{VPI_INCLUDE_DIR} CACHE PATH "Path to the VPI include directory")

# Ensure the variable is set
if (NOT vpi_include_dir)
    message(FATAL_ERROR "VPI_INCLUDE_DIR environment variable must be set or provided manually.")
endif ()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall -I${vpi_include_dir}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23")

# Add subdirectories for internal components
add_subdirectory(src/core)
add_subdirectory(src/testbase)
add_subdirectory(src/scheduler)
add_subdirectory(src/testmanager)

# Option to build shared library (.so) for internal testing (not installed)
option(BUILD_SHARED_LIBRARY "Build the shared library (.so) for internal testing" ON)

if (BUILD_SHARED_LIBRARY)
    # Build the shared library for internal use only
    add_library(rapidvpi.vpi SHARED src/entry.cpp)

    # Include directories for internal components and VPI
    target_include_directories(rapidvpi.vpi PRIVATE
            ${vpi_include_dir}  # For VPI headers
            ${CMAKE_CURRENT_SOURCE_DIR}/src/core  # Core headers
            ${CMAKE_CURRENT_SOURCE_DIR}/src/scheduler  # Scheduler headers
            ${CMAKE_CURRENT_SOURCE_DIR}/src/testbase  # Testbase headers
            ${CMAKE_CURRENT_SOURCE_DIR}/src/testmanager  # Testmanager headers
    )

    # IMPORTANT for Questa:
    #   Do NOT link any external vpi/libveriuser library.
    #   The simulator provides VPI symbols internally.
    target_link_libraries(rapidvpi.vpi PRIVATE core testbase scheduler testmanager)
endif ()

# Install public headers and source files under rapidvpi folder to keep them organized
install(DIRECTORY src/core/ DESTINATION include/rapidvpi/core FILES_MATCHING PATTERN "*.hpp")
install(DIRECTORY src/scheduler/ DESTINATION include/rapidvpi/scheduler FILES_MATCHING PATTERN "*.hpp")
install(DIRECTORY src/testmanager/ DESTINATION include/rapidvpi/testmanager FILES_MATCHING PATTERN "*.hpp")
install(DIRECTORY src/testbase/ DESTINATION include/rapidvpi/testbase FILES_MATCHING PATTERN "*.hpp")

# Install .cpp source files for user integration
install(DIRECTORY src/core/ DESTINATION src/rapidvpi/core FILES_MATCHING PATTERN "*.cpp")
install(DIRECTORY src/scheduler/ DESTINATION src/rapidvpi/scheduler FILES_MATCHING PATTERN "*.cpp")
install(DIRECTORY src/testmanager/ DESTINATION src/rapidvpi/testmanager FILES_MATCHING PATTERN "*.cpp")
install(DIRECTORY src/testbase/ DESTINATION src/rapidvpi/testbase FILES_MATCHING PATTERN "*.cpp")

# Ensure entry.cpp is included in the installed sources (organized under rapidvpi/src)
install(FILES src/entry.cpp DESTINATION src/rapidvpi)

# Export the targets so users can link against them
install(TARGETS rapidvpi.vpi EXPORT rapidvpiTargets DESTINATION lib/rapidvpi)

# Install the export file, which creates rapidvpiTargets.cmake
install(EXPORT rapidvpiTargets FILE rapidvpiTargets.cmake DESTINATION lib/cmake/rapidvpi NAMESPACE rapidvpi::)

# Generate and install the config and version files for find_package
configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/rapidvpiConfig.cmake
        INSTALL_DESTINATION lib/cmake/rapidvpi
)

write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/rapidvpiConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/rapidvpiConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/rapidvpiConfigVersion.cmake"
        DESTINATION lib/cmake/rapidvpi)