# LED blink + ADC internal-channel demo for the STM32U575ZIT6 "nucleo-u575"
# board. Chases the three board LEDs (LD1 green PC7 / LD2 blue PB7 / LD3 red
# PG2, high-active) and periodically samples the ADC1 internal channels
# (VREFINT, temperature sensor, VBAT/4) and prints them over USART1 (PA9/PA10)
# at 115200 baud (ST-Link VCP). Runs at 160 MHz from the internal MSI.
#
# Pico-style workflow:
#   bash build.sh          # == cmake -G Ninja -S . -B build && ninja -C build
#   ninja -C build flash   # programs the board (probe-rs, ST-Link SWD)

cmake_minimum_required(VERSION 3.13)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/arm-none-eabi-toolchain.cmake")
endif()

project(blink_hello C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/adc_internal.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32u575_board.cmake)
stm32u575_apply_board(${PROJECT_NAME}.elf "-O1")


include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
