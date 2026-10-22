cmake_minimum_required(VERSION 3.19)

set(MBED_PATH ${CMAKE_CURRENT_SOURCE_DIR}/mbed-os CACHE INTERNAL "")
set(MBED_CONFIG_PATH ${CMAKE_CURRENT_BINARY_DIR} CACHE INTERNAL "")
set(MBED_TARGET LPC1768 CACHE STRING "Mbed target")
set(APP_TARGET_NAME laoslaser)

include(${MBED_PATH}/tools/cmake/mbed_toolchain_setup.cmake)
project(laoslaser LANGUAGES C CXX ASM)
include(${MBED_PATH}/tools/cmake/mbed_project_setup.cmake)

# Flash through the on-board CMSIS-DAP interface chip with pyOCD. The default
# (MBED) copies the .bin to the USB mass storage drive, which is not always
# mounted. Override with -DUPLOAD_METHOD=MBED if preferred.
set(UPLOAD_METHOD PYOCD CACHE STRING "Upload method")
set(PYOCD_UPLOAD_ENABLED TRUE)
set(PYOCD_TARGET_NAME lpc1768)
set(PYOCD_CLOCK_SPEED 1M)

add_subdirectory(${MBED_PATH})

# Step 1: bring-up program, blinks LEDs and prints over the USB serial port.
add_executable(hello hello/main.cpp)
target_link_libraries(hello PRIVATE mbed-os)
mbed_set_post_build(hello)

# Step 2: motion/IO/display port. Menu, SD, Ethernet/TFTP follow in later steps.
find_package(Git QUIET)
set(GIT_HASH "")
if(GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short=6 HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()

add_executable(laser
    src/main_bringup.cpp
    src/global.cpp
    src/ConfigFile/ConfigFile.cpp
    src/LaosMotion/pins.cpp
    src/LaosMotion/LaosMotion.cpp
    src/LaosMotion/grbl/planner.cpp
    src/LaosMotion/grbl/stepper.cpp
    src/LaosMotion/grbl/fixedpt.cpp
    src/LaosIO/LaosIO.cpp
    src/LaosExtent/LaosExtent.cpp
    src/LaosDisplay/LaosDisplay.cpp
    src/LaosDisplay/MODI2C.cpp
    src/LaosDisplay/MODI2C_IRQ.cpp
)
target_include_directories(laser PRIVATE
    src
    src/ConfigFile
    src/LaosMotion
    src/LaosMotion/grbl
    src/LaosIO
    src/LaosExtent
    src/LaosDisplay
)
target_compile_definitions(laser PRIVATE __GIT_HASH="${GIT_HASH}")
target_link_libraries(laser PRIVATE mbed-os)
mbed_set_post_build(laser)

# Ticker rate/jitter measurement for the stepper timer (risk check)
add_executable(ticktest ticktest/main.cpp)
target_link_libraries(ticktest PRIVATE mbed-os)
mbed_set_post_build(ticktest)
