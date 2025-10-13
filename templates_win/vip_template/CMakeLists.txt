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

# VPI include dir (contains vpi_user.h). You pass it via -Dvpi_include_dir=/home/... or env VPI_INCLUDE_DIR.
set(vpi_include_dir "$ENV{VPI_INCLUDE_DIR}" CACHE PATH "Path to the VPI include directory (contains vpi_user.h)")
if(NOT vpi_include_dir OR vpi_include_dir STREQUAL "")
  message(FATAL_ERROR "VPI_INCLUDE_DIR must be set (contains vpi_user.h). Example: -Dvpi_include_dir=/home/arcspecter/tools/iverilog")
endif()

# Find RapidVPI (installed at /usr/local/lib/cmake/rapidvpi)
# If Windows CMake is driving, pass: -Drapidvpi_DIR=F:/msys2/usr/local/lib/cmake/rapidvpi
find_package(rapidvpi REQUIRED CONFIG)

# Your sources
set(TEST_SOURCES
  src/test/test.cpp
  src/test/modules/test_impl.cpp
)

# Build your VPI plugin
add_library(${PROJECT_NAME} SHARED ${TEST_SOURCES})

# ---- Ensure vlog_startup_routines is present: compile RapidVPI's entry.cpp into THIS plugin
# Primary (MSYS/Linux) path:
set(RAPIDVPI_ENTRY_MSYS "/usr/local/src/rapidvpi/entry.cpp")

if(EXISTS "${RAPIDVPI_ENTRY_MSYS}")
  target_sources(${PROJECT_NAME} PRIVATE "${RAPIDVPI_ENTRY_MSYS}")
elseif(WIN32)
  # If Windows CMake is used with MSYS files installed, convert the MSYS path to Windows
  execute_process(
    COMMAND bash -lc "cygpath -m \"/usr/local/src/rapidvpi/entry.cpp\""
    OUTPUT_VARIABLE RAPIDVPI_ENTRY_WIN
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(EXISTS "${RAPIDVPI_ENTRY_WIN}")
    target_sources(${PROJECT_NAME} PRIVATE "${RAPIDVPI_ENTRY_WIN}")
  else()
    message(FATAL_ERROR "Cannot locate RapidVPI entry.cpp (checked ${RAPIDVPI_ENTRY_MSYS} and ${RAPIDVPI_ENTRY_WIN})")
  endif()
else()
  message(FATAL_ERROR "Cannot locate RapidVPI entry.cpp at ${RAPIDVPI_ENTRY_MSYS}")
endif()

# Includes — VPI first; RapidVPI headers come from the sdk target automatically
target_include_directories(${PROJECT_NAME} PRIVATE
  ${vpi_include_dir}
  ${CMAKE_SOURCE_DIR}/src/test
  ${CMAKE_SOURCE_DIR}/src/test/modules
)

# Link the RapidVPI SDK (prebuilt lib; avoids recompiling SDK sources)
target_link_libraries(${PROJECT_NAME} PRIVATE rapidvpi::sdk)

# Try to link a simulator import lib for vpi_* (needed on Windows DLL linkers)
# On MSYS you typically have /usr/lib/libvpi.a from Icarus
find_library(VPI_IMPORT_LIB NAMES vpi vvp
  HINTS /usr/lib /usr/local/lib /mingw64/lib /mingw32/lib)
if(VPI_IMPORT_LIB)
  target_link_libraries(${PROJECT_NAME} PRIVATE "${VPI_IMPORT_LIB}")
elseif(WIN32)
  # Last resort on Windows: allow unresolved symbols in the .vpi (resolved by simulator at load)
  target_link_options(${PROJECT_NAME} PRIVATE -Wl,--unresolved-symbols=ignore-in-shared-libs)
endif()

# Merge extra objects
add_subdirectory(src/test/modules/test_impl2)

# Make output a proper .vpi with no lib/msys prefix
set_target_properties(${PROJECT_NAME} PROPERTIES
  PREFIX ""
  SUFFIX ".vpi"
  OUTPUT_NAME "vip_template"
)

# Friendly Windows/MinGW link behavior
if(MINGW OR WIN32)
  target_link_options(${PROJECT_NAME} PRIVATE -Wl,--export-all-symbols -Wl,--enable-auto-import)
endif()
