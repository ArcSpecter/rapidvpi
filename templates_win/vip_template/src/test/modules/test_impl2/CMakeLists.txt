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

# src/test/modules/test_impl2/CMakeLists.txt

cmake_minimum_required(VERSION 3.10)

# Name for this submodule
set(TEST_SUBFOLDER test_impl2)

# Build as an OBJECT library so its .o files are merged into vip_template
add_library(${TEST_SUBFOLDER} OBJECT
  test_impl2.cpp
)

# We expect the parent to pass -Dvpi_include_dir=... (or export VPI_INCLUDE_DIR)
if(NOT DEFINED vpi_include_dir OR vpi_include_dir STREQUAL "")
  message(FATAL_ERROR "vpi_include_dir is not set. Configure vip_template with -Dvpi_include_dir=/path/to/iverilog")
endif()

# Include order matters: put vpi_include_dir FIRST so <vpi_user.h> resolves correctly.
target_include_directories(${TEST_SUBFOLDER} PRIVATE
  ${vpi_include_dir}                         # Icarus VPI headers (e.g., /home/arcspecter/tools/iverilog)
  ${CMAKE_CURRENT_SOURCE_DIR}/..             # for test_impl2.hpp → test.hpp
  ${CMAKE_SOURCE_DIR}/src/test               # base test dir
  /usr/local/include/rapidvpi/core
  /usr/local/include/rapidvpi/scheduler
  /usr/local/include/rapidvpi/testbase
  /usr/local/include/rapidvpi/testmanager
)

# PIC for merging into a shared lib
set_property(TARGET ${TEST_SUBFOLDER} PROPERTY POSITION_INDEPENDENT_CODE ON)

# Contribute these objects to the main vip_template shared library
target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:${TEST_SUBFOLDER}>)

