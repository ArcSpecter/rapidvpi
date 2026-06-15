# vip_common - shared utilities used by VIP submodules and the top-level project
#
# This bundle is intended to be a *self-contained* dependency for other VIPs
# under ./ext (vip_axis/vip_gmii/vip_rgmii/...). Nothing here should depend on
# the master project's src/test/* implementation details.

set(TEST_SUBFOLDER vip_common)

add_library(${TEST_SUBFOLDER} OBJECT
        common/common.cpp
        common/logger.cpp
        scoreboard/scoreboard.cpp

        runner/runner.cpp

        agents/clock/clock.cpp
        agents/por/por.cpp
)

target_include_directories(${TEST_SUBFOLDER} PRIVATE
        ${CMAKE_SOURCE_DIR}/ext               # allows includes like "vip_common/..." from anywhere
        /usr/local/include/rapidvpi/core
        /usr/local/include/rapidvpi/scheduler
        /usr/local/include/rapidvpi/testbase
        /usr/local/include/rapidvpi/testmanager
        ${vpi_include_dir}
)

set_property(TARGET ${TEST_SUBFOLDER} PROPERTY POSITION_INDEPENDENT_CODE ON)

target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:${TEST_SUBFOLDER}>)
