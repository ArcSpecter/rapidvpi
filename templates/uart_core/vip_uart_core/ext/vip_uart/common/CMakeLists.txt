# MIT License
#
# Copyright (c) 2026 Rovshan Rustamov
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

set(TEST_SUBFOLDER vip_uart_common)

add_library(${TEST_SUBFOLDER} OBJECT
        uart_params.cpp
)

target_include_directories(${TEST_SUBFOLDER} PRIVATE
        ${CMAKE_SOURCE_DIR}/ext
        /usr/local/include/rapidvpi/core
        /usr/local/include/rapidvpi/scheduler
        /usr/local/include/rapidvpi/testbase
        /usr/local/include/rapidvpi/testmanager
        ${vpi_include_dir}
)

set_property(TARGET ${TEST_SUBFOLDER} PROPERTY POSITION_INDEPENDENT_CODE ON)

target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:${TEST_SUBFOLDER}>)
