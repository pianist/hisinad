cmake_minimum_required(VERSION 2.8.12)
project(hisinad C)

set(CMAKE_C_STANDARD 99)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(HiSiliconSDK REQUIRED)
find_package(Threads REQUIRED)

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${VENDOR_SDK_INCLUDE_DIRS})


add_executable(test_cfg_read test/test_cfg_read.c cfg/common.c)
add_executable(test_cfg_sensor test/test_cfg_sensor.c cfg/common.c cfg/sensor_config.c cfg/sensor_cfg_print.c)

add_executable(test_pumap_vi test/test_pumap_vi.c pumap/vi.c)

if (CMAKE_SYSTEM_PROCESSOR STREQUAL armv5l)
message(STATUS "Architecture: ${CMAKE_SYSTEM_PROCESSOR}")

endif()

