# MIT License
#
# Copyright (c) 2024 Rovshan Rustamov
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# scripts/cmake/rtl_dependency_helpers.cmake
#
# Helper macros for importing already-checked-out RTL dependency submodules.
#
# Expected dependency layout:
#
#   <repo>/ext/<dependency>/scripts/cmake/questa_modules.cmake
#
# Caller requirements:
#
#   set(EXT_DIR ${CMAKE_CURRENT_LIST_DIR}/ext)
#   include(${CMAKE_CURRENT_LIST_DIR}/scripts/cmake/rtl_dependency_helpers.cmake)
#
# This file intentionally uses macros instead of functions so included dependency
# import files can update caller-scope variables such as QUESTA_INIT_COMMANDS.

if(DEFINED RTL_DEPENDENCY_HELPERS_INCLUDED)
  return()
endif()
set(RTL_DEPENDENCY_HELPERS_INCLUDED TRUE)

macro(rtl_import_module module_name)
  if("${module_name}" STREQUAL "")
    message(FATAL_ERROR
      "rtl_import_module called with an empty module name"
    )
  endif()

  if(NOT DEFINED EXT_DIR)
    message(FATAL_ERROR
      "EXT_DIR is not defined before rtl_import_module(${module_name}). "
      "Define EXT_DIR in the top-level CMakeLists.txt, normally as "
      "\${CMAKE_CURRENT_LIST_DIR}/ext."
    )
  endif()

  if("${EXT_DIR}" STREQUAL "")
    message(FATAL_ERROR
      "EXT_DIR is empty before rtl_import_module(${module_name})"
    )
  endif()

  set(_rtl_dep_name "${module_name}")
  set(_rtl_dep_dir "${EXT_DIR}/${_rtl_dep_name}")
  set(_rtl_dep_import_file "${_rtl_dep_dir}/scripts/cmake/questa_modules.cmake")

  get_property(_rtl_dep_imported_modules GLOBAL PROPERTY RTL_IMPORTED_MODULES)
  if(NOT _rtl_dep_imported_modules)
    set(_rtl_dep_imported_modules "")
  endif()

  list(FIND _rtl_dep_imported_modules "${_rtl_dep_name}" _rtl_dep_index)

  if(_rtl_dep_index EQUAL -1)
    if(NOT EXISTS "${_rtl_dep_dir}")
      message(FATAL_ERROR
        "Missing RTL dependency submodule: ${_rtl_dep_dir}\n"
        "Expected imported submodule under EXT_DIR: ${EXT_DIR}"
      )
    endif()

    if(NOT IS_DIRECTORY "${_rtl_dep_dir}")
      message(FATAL_ERROR
        "RTL dependency path exists but is not a directory: ${_rtl_dep_dir}"
      )
    endif()

    if(NOT EXISTS "${_rtl_dep_import_file}")
      message(FATAL_ERROR
        "Missing Questa module import file for RTL dependency ${_rtl_dep_name}:\n"
        "  ${_rtl_dep_import_file}\n"
        "Expected standard filename:\n"
        "  scripts/cmake/questa_modules.cmake"
      )
    endif()

    message(STATUS "Importing RTL dependency: ${_rtl_dep_name}")

    set_property(GLOBAL APPEND PROPERTY RTL_IMPORTED_MODULES "${_rtl_dep_name}")

    include("${_rtl_dep_import_file}")
  else()
    message(STATUS "RTL dependency already imported: ${_rtl_dep_name}")
  endif()

  unset(_rtl_dep_name)
  unset(_rtl_dep_dir)
  unset(_rtl_dep_import_file)
  unset(_rtl_dep_imported_modules)
  unset(_rtl_dep_index)
endmacro()

macro(rtl_import_modules)
  foreach(_rtl_dep_batch_name ${ARGN})
    rtl_import_module(${_rtl_dep_batch_name})
  endforeach()

  unset(_rtl_dep_batch_name)
endmacro()
