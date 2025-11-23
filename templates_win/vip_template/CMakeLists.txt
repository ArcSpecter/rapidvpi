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
cmake_minimum_required(VERSION 3.10)

# Project
set(PROJECT_NAME vip_template)
project(${PROJECT_NAME} VERSION 0.0.1 LANGUAGES CXX)

# C++ std
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# VPI include dir (contains vpi_user.h).
set(vpi_include_dir "$ENV{VPI_INCLUDE_DIR}" CACHE PATH
  "Path to the VPI include directory (contains vpi_user.h)")
if(NOT vpi_include_dir OR vpi_include_dir STREQUAL "")
  message(FATAL_ERROR
    "VPI_INCLUDE_DIR / vpi_include_dir must be set (contains vpi_user.h). "
    "Example (Linux/Questa): -Dvpi_include_dir=/opt/questa_fse/include"
  )
endif()

# On Windows we also need the PLI/VPI import library dir (mtipli/libmtipli)
if(WIN32)
  set(vpi_lib_dir "$ENV{VPI_LIB_DIR}" CACHE PATH
    "Path to the VPI/PLI library directory (contains libmtipli.a/mtipli.lib)")
  if(NOT vpi_lib_dir OR vpi_lib_dir STREQUAL "")
    message(FATAL_ERROR
      "VPI_LIB_DIR / vpi_lib_dir must be set on Windows "
      "(e.g. C:/temp_backup/eda/quartus_24_1_lite/questa_fse/win64).")
  endif()
endif()

# Find RapidVPI (Linux: /usr/local/lib/cmake/rapidvpi
# Windows: main_win/install/lib/cmake/rapidvpi)
find_package(rapidvpi REQUIRED CONFIG)

# --------------------------------------------------------------------
# Locate entry.cpp from the installed RapidVPI layout
# --------------------------------------------------------------------
get_filename_component(RAPIDVPI_CONFIG_DIR "${rapidvpi_DIR}" ABSOLUTE)
get_filename_component(RAPIDVPI_LIB_CMAKE_DIR "${RAPIDVPI_CONFIG_DIR}" DIRECTORY) # .../lib/cmake
get_filename_component(RAPIDVPI_LIB_DIR       "${RAPIDVPI_LIB_CMAKE_DIR}" DIRECTORY) # .../lib
get_filename_component(RAPIDVPI_PREFIX        "${RAPIDVPI_LIB_DIR}" DIRECTORY) # <prefix>

set(RAPIDVPI_ENTRY "${RAPIDVPI_PREFIX}/src/rapidvpi/entry.cpp")

if(EXISTS "${RAPIDVPI_ENTRY}")
  message(STATUS "Using RapidVPI entry.cpp at: ${RAPIDVPI_ENTRY}")
else()
  message(FATAL_ERROR
    "Cannot locate RapidVPI entry.cpp at ${RAPIDVPI_ENTRY}. "
    "Check your RapidVPI install prefix and that sources were installed.")
endif()

# Your sources
set(TEST_SOURCES
  src/test/test.cpp
  src/test/modules/test_impl.cpp
  "${RAPIDVPI_ENTRY}" # bring vlog_startup_routines into this plugin
)

# Build your VPI plugin
add_library(${PROJECT_NAME} SHARED ${TEST_SOURCES})

# Includes — VPI first; add RapidVPI prefix/include so <rapidvpi/...> works
target_include_directories(${PROJECT_NAME} PRIVATE
  ${vpi_include_dir}
  ${CMAKE_SOURCE_DIR}/src/test
  ${CMAKE_SOURCE_DIR}/src/test/modules
  "${RAPIDVPI_PREFIX}/include"
)

# Link the RapidVPI SDK (prebuilt lib)
target_link_libraries(${PROJECT_NAME} PRIVATE rapidvpi::sdk)

# --------------------------------------------------------------------
# VPI/PLI import library
# --------------------------------------------------------------------
if(WIN32)
  find_library(VPI_IMPORT_LIB
    NAMES mtipli libmtipli
    HINTS "${vpi_lib_dir}"
  )

  if(VPI_IMPORT_LIB)
    message(STATUS "Using VPI/PLI import library: ${VPI_IMPORT_LIB}")
    target_link_libraries(${PROJECT_NAME} PRIVATE "${VPI_IMPORT_LIB}")
  else()
    message(FATAL_ERROR
      "VPI/PLI import library not found in VPI_LIB_DIR='${vpi_lib_dir}'. "
      "Expected libmtipli.a or mtipli.lib from Questa win64.")
  endif()
else()
  find_library(VPI_IMPORT_LIB NAMES vpi vvp
    HINTS /usr/lib /usr/local/lib /mingw64/lib /mingw32/lib)

  if(VPI_IMPORT_LIB)
    message(STATUS "Using VPI import library: ${VPI_IMPORT_LIB}")
    target_link_libraries(${PROJECT_NAME} PRIVATE "${VPI_IMPORT_LIB}")
  endif()
endif()

# Merge extra objects
add_subdirectory(src/test/modules/test_impl2)

# Make output a proper .vpi with no lib- prefix
set_target_properties(${PROJECT_NAME} PROPERTIES
  PREFIX ""
  SUFFIX ".vpi"
  OUTPUT_NAME "vip_template"
)

if(MINGW OR WIN32)
  target_link_options(${PROJECT_NAME} PRIVATE -Wl,--export-all-symbols -Wl,--enable-auto-import)
endif()
