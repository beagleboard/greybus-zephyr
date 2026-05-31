cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

set(DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
project(fake_camera_test)

#absolute path
get_filename_component(GB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../.. ABSOLUTE)

#compiling both the test and driver
target_sources(app PRIVATE 
    src/main.c
    ${GB_ROOT}/drivers/video/fake_camera.c
)
