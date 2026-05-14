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
# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(ProiectSM C CXX ASM)

set(PICO_NO_COPRO_DIS 1)

pico_sdk_init()

add_executable(ProiectSM
    ProiectSM.c
)

pico_set_program_name(ProiectSM "ProiectSM")
pico_set_program_version(ProiectSM "0.1")

pico_enable_stdio_uart(ProiectSM 0)
pico_enable_stdio_usb(ProiectSM 1)

target_include_directories(ProiectSM PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(ProiectSM
    pico_stdlib
    pico_cyw43_arch_lwip_poll
    hardware_i2c
    hardware_adc
)

pico_add_extra_outputs(ProiectSM)