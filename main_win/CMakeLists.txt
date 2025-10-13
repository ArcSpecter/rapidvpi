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
# VPI_LIB_DIR: pointing to the library location for VPI library (since initially
# RapidVPI is designed to work with iverilog that path would be "path/to/iverilog/libveriuser")
# VPI_INCLUDE_DIR: pointing to the location where the "vpi_user.h" file is located (again for
# the case of iverilog it is "path/to/iverilog")

cmake_minimum_required(VERSION 3.10)

project(rapidvpi VERSION 0.0.1 LANGUAGES CXX)

include(CMakePackageConfigHelpers)

# -------------------------------
# Inputs (env or -D)
# -------------------------------
set(vpi_lib_dir     "$ENV{VPI_LIB_DIR}"     CACHE PATH "Path to the VPI library directory (contains libvpi.*)")
set(vpi_include_dir "$ENV{VPI_INCLUDE_DIR}" CACHE PATH "Path to the VPI include directory (contains vpi_user.h)")

if(NOT vpi_include_dir)
  message(FATAL_ERROR "VPI_INCLUDE_DIR must be set (contains vpi_user.h).")
endif()
if(NOT vpi_lib_dir)
  message(FATAL_ERROR "VPI_LIB_DIR must be set (directory with libvpi.*).")
endif()

# -------------------------------
# Global compile settings
# -------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# For MSYS/MinGW Windows builds, make linking friendlier
if(MINGW OR WIN32)
  add_link_options(-Wl,--enable-auto-import)
endif()

# We’ll need VPI headers both to build the SDK and the plugin
include_directories(${vpi_include_dir})

# -------------------------------
# SDK library (the thing downstream projects will link)
# Exposes imported target name: rapidvpi::sdk
# -------------------------------
add_library(rapidvpi_sdk STATIC
  src/core/core.cpp
  src/scheduler/scheduler.cpp
  src/testmanager/testmanager.cpp
  src/testbase/awaitchange.cpp
  src/testbase/awaitread.cpp
  src/testbase/awaitwrite.cpp
  src/testbase/testbase.cpp
  src/testbase/utility.cpp
  src/entry.cpp       # entry points live in the plugin, but we keep them in SDK so user plugins get them via the lib
)

# Public headers for users of the SDK
target_include_directories(rapidvpi_sdk
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/core>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/scheduler>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/testmanager>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/testbase>
    $<INSTALL_INTERFACE:include/rapidvpi/core>
    $<INSTALL_INTERFACE:include/rapidvpi/scheduler>
    $<INSTALL_INTERFACE:include/rapidvpi/testmanager>
    $<INSTALL_INTERFACE:include/rapidvpi/testbase>
)

# Make the exported/imported name nice: rapidvpi::sdk
set_target_properties(rapidvpi_sdk PROPERTIES EXPORT_NAME sdk)
add_library(rapidvpi::sdk ALIAS rapidvpi_sdk)

# -------------------------------
# Optional plugin (.vpi/.dll) for internal testing
# Imported name remains: rapidvpi::rapidvpi.vpi (legacy)
# -------------------------------
option(BUILD_SHARED_PLUGIN "Build the RapidVPI test plugin (.vpi) for internal testing" ON)

if(BUILD_SHARED_PLUGIN)
  # Keep a simple CMake target name without dots, but export it as 'rapidvpi.vpi'
  add_library(rapidvpi_plugin SHARED
    src/entry.cpp
    src/core/user_factory_stub.cpp    # plugin-only stub for tests
  )

  # Produce rapidvpi.vpi as the filename, no 'lib' prefix
  set_target_properties(rapidvpi_plugin PROPERTIES
    OUTPUT_NAME "rapidvpi"
    PREFIX ""
    SUFFIX ".vpi"
    EXPORT_NAME "rapidvpi.vpi"   # imported name: rapidvpi::rapidvpi.vpi
  )

  # Link SDK and VPI import library
  # (Downstream user plugins will normally only link the SDK + their own code)
  find_library(VPI_LIBRARY_NAME NAMES vpi
    HINTS ${vpi_lib_dir} /usr/local/lib /usr/lib /mingw64/lib /mingw32/lib)
  if(NOT VPI_LIBRARY_NAME)
    message(FATAL_ERROR "VPI library not found (searched in: ${vpi_lib_dir}, /usr/local/lib, /usr/lib, /mingw64/lib, /mingw32/lib)")
  endif()

  target_link_libraries(rapidvpi_plugin PRIVATE
    rapidvpi_sdk
    ${VPI_LIBRARY_NAME}
  )

  if(MINGW OR WIN32)
    target_link_options(rapidvpi_plugin PRIVATE -Wl,--export-all-symbols)
  endif()
endif()

# -------------------------------
# Install headers
# -------------------------------
install(DIRECTORY src/core/        DESTINATION include/rapidvpi/core        FILES_MATCHING PATTERN "*.hpp")
install(DIRECTORY src/scheduler/   DESTINATION include/rapidvpi/scheduler   FILES_MATCHING PATTERN "*.hpp")
install(DIRECTORY src/testmanager/ DESTINATION include/rapidvpi/testmanager FILES_MATCHING PATTERN "*.hpp")
install(DIRECTORY src/testbase/    DESTINATION include/rapidvpi/testbase    FILES_MATCHING PATTERN "*.hpp")

# (Optional) install sources for legacy “compile sources in user project” flow
option(INSTALL_SDK_SOURCES "Install RapidVPI SDK sources for legacy integration" ON)
if(INSTALL_SDK_SOURCES)
  install(DIRECTORY src/core/        DESTINATION src/rapidvpi/core
          FILES_MATCHING PATTERN "*.cpp" PATTERN "user_factory_stub.cpp" EXCLUDE)
  install(DIRECTORY src/scheduler/   DESTINATION src/rapidvpi/scheduler  FILES_MATCHING PATTERN "*.cpp")
  install(DIRECTORY src/testmanager/ DESTINATION src/rapidvpi/testmanager FILES_MATCHING PATTERN "*.cpp")
  install(DIRECTORY src/testbase/    DESTINATION src/rapidvpi/testbase   FILES_MATCHING PATTERN "*.cpp")
  install(FILES src/entry.cpp DESTINATION src/rapidvpi)
endif()

# -------------------------------
# Install SDK lib
# -------------------------------
install(TARGETS rapidvpi_sdk
  EXPORT rapidvpiTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

# -------------------------------
# Install plugin (if built)
# -------------------------------
if(BUILD_SHARED_PLUGIN)
  install(TARGETS rapidvpi_plugin
    EXPORT rapidvpiTargets
    RUNTIME DESTINATION lib/rapidvpi
    LIBRARY DESTINATION lib/rapidvpi
    ARCHIVE DESTINATION lib/rapidvpi
  )
endif()

# -------------------------------
# Export targets and package config
# -------------------------------
install(EXPORT rapidvpiTargets
  FILE rapidvpiTargets.cmake
  NAMESPACE rapidvpi::
  DESTINATION lib/cmake/rapidvpi
)

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
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/rapidvpiConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/rapidvpiConfigVersion.cmake"
  DESTINATION lib/cmake/rapidvpi
)
