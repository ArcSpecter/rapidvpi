# Define a local variable for the module name
set(TEST_SUBFOLDER test_impl2)

# Create an object library for the module
add_library(${TEST_SUBFOLDER} OBJECT
        # list all .cpp files for the specific test module
        test_impl2.cpp
)

# Add the include directories for this target
target_include_directories(${TEST_SUBFOLDER} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..  # To include test.hpp from the parent directory
        ${CMAKE_SOURCE_DIR}/src/test  # Include the base test directory for all targets
        /usr/local/include/rapidvpi/core
        /usr/local/include/rapidvpi/scheduler
        /usr/local/include/rapidvpi/testbase
        /usr/local/include/rapidvpi/testmanager
        /usr/local/include/iverilog  # Include the VPI headers
)

# Ensure position-independent code for shared library compatibility
set_property(TARGET ${TEST_SUBFOLDER} PROPERTY POSITION_INDEPENDENT_CODE ON)

# Add the compiled object files from this module to the ${PROJECT_NAME} shared library
target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:${TEST_SUBFOLDER}>)
