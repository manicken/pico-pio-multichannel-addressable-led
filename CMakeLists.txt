# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(pico-pio-multichannel-addressable-led C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(pico-pio-multichannel-addressable-led pico-pio-multichannel-addressable-led.c )

pico_set_program_name(pico-pio-multichannel-addressable-led "pico-pio-multichannel-addressable-led")
pico_set_program_version(pico-pio-multichannel-addressable-led "0.1")

# Generate PIO header
pico_generate_pio_header(pico-pio-multichannel-addressable-led ${CMAKE_CURRENT_LIST_DIR}/blink.pio)
pico_generate_pio_header(pico-pio-multichannel-addressable-led ${CMAKE_CURRENT_LIST_DIR}/ws28xx-16ch.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(pico-pio-multichannel-addressable-led 0)
pico_enable_stdio_usb(pico-pio-multichannel-addressable-led 1)

# Add the standard library to the build
target_link_libraries(pico-pio-multichannel-addressable-led
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(pico-pio-multichannel-addressable-led PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(pico-pio-multichannel-addressable-led 
        pico_stdlib
        hardware_pio
        hardware_dma
        hardware_gpio
        pico_multicore        )

pico_add_extra_outputs(pico-pio-multichannel-addressable-led)

