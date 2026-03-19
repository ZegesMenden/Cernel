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
set(toolchainVersion RISCV_ZCB_RPI_2_2_0_3)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(pico-kernel C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Kernel module
add_subdirectory(kernel)

# Add executable. Default name is the project name, version 0.1
add_executable(pico-kernel main.cpp )

target_link_libraries(pico-kernel PRIVATE
    pico_kernel)

target_compile_definitions(pico_kernel PUBLIC LOG_ENABLE)
target_compile_definitions(pico_kernel PUBLIC LOG_USE_COLORS)
target_compile_definitions(pico_kernel PUBLIC LOG_USE_TIMESTAMP)

# target_compile_definitions(pico_kernel PUBLIC LOG_LEVEL=1) # Log level error
# target_compile_definitions(pico_kernel PUBLIC LOG_LEVEL=2) # Log level warn
target_compile_definitions(pico_kernel PUBLIC LOG_LEVEL=4) # Log level info
# target_compile_definitions(pico_kernel PUBLIC LOG_LEVEL=8) # Log level 


pico_set_program_name(pico-kernel "pico-kernel")
pico_set_program_version(pico-kernel "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(pico-kernel 0)
pico_enable_stdio_usb(pico-kernel 1)

pico_add_extra_outputs(pico-kernel)

