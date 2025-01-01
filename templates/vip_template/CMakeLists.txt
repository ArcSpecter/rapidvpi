cmake_minimum_required(VERSION 3.10)

# Set the project and target name
set(PROJECT_NAME vip_template)
project(${PROJECT_NAME} VERSION 0.0.1)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 23)

# Set the VPI include path (e.g., from Icarus Verilog, or another simulator)
set(vpi_include_dir $ENV{VPI_INCLUDE_DIR} CACHE PATH "Path to the VPI include directory")

# Ensure the variable is set
if (NOT vpi_include_dir)
    message(FATAL_ERROR "VPI_INCLUDE_DIR environment variable must be set or provided manually.")
endif()

# Set the path where rapidvpiConfig.cmake is located
set(CMAKE_PREFIX_PATH "/usr/local/lib/cmake/rapidvpi")

# Find the rapidvpi library (rapidvpi) using find_package
find_package(rapidvpi REQUIRED)

# Add user's test files (from the src/test and src/test/modules directories)
set(TEST_SOURCES
        src/test/test.cpp
        src/test/modules/test_impl.cpp
)

# Create the shared library using the project name (ONLY user's test code)
add_library(${PROJECT_NAME} SHARED ${TEST_SOURCES})

# Specify include directories (for user's test folder, and VPI headers)
target_include_directories(${PROJECT_NAME} PRIVATE
        /usr/local/include/rapidvpi/core
        /usr/local/include/rapidvpi/scheduler
        /usr/local/include/rapidvpi/testbase
        /usr/local/include/rapidvpi/testmanager
        src/test  # User's test folder
        src/test/modules  # User's test modules folder
        ${vpi_include_dir}  # Path to the VPI headers (e.g., from Icarus Verilog)
)

# Add the subdirectory for test_impl2, which has its own CMakeLists.txt
add_subdirectory(src/test/modules/test_impl2)

# Link against the imported target from the rapidvpi library
target_link_libraries(${PROJECT_NAME} PRIVATE rapidvpi::rapidvpi.vpi)

# Add VPI library for simulation
target_link_libraries(${PROJECT_NAME} PRIVATE vpi)
