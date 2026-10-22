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
