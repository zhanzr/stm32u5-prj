# nv3030b_md183_240x284_cst816d: NV3030B 1.83" 240x284 LCM (TK018F3716 module) driven
# over plain single-lane SPI + CST816D touch on hardware I2C1, on the
# NUCLEO-U575ZI-Q @ 160 MHz. Ported from the ch32v307 reference
# nv3030b_md183_240x284_cst816d (same module, vendor-verbatim registers).
# The OCTOSPI/QuadSPI path was dropped: its HAL transmit is byte-at-a-time
# and could not be made fast enough to exercise a wide data phase.
# Wrapped command = CS frame with the 4-byte header 02 00 <cmd> 00 then the
# parameter/pixel bytes; the demo alternates SOFT (bit-bang) and HW (SPI1)
# on the same three wires so a difference is a difference in the SPI engine.
# Wiring: CS=PA4 SCK=PA5 MOSI=PA7 (SPI1 AF5; MISO not connected - the
# driver is write-only by design); touch CST816D on I2C1 (SCL=PB8, SDA=PB9,
# AF4, unchanged).
#
#   bash build.sh          # == cmake -G Ninja -S . -B build && ninja -C build
#   ninja -C build flash   # programs the board (probe-rs; ST-Link SWD)

cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/arm-none-eabi-toolchain.cmake")
endif()

project(nv3030b_md183_240x284_cst816d C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Board include first: it resolves STM32U5_HAL_SRC (the vendored repo-root
# drivers/) which the source list below references.
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32u575_board.cmake)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/interface.c
    src/lcd.c
    src/touch.c
    src/lcd_fonts.c
    src/lcd_font_1608.c
    # SPI (display bus: soft bit-bang or SPI1) + I2C HAL (touch on I2C1)
    ${STM32U5_HAL_SRC}/stm32u5xx_hal_spi.c
    ${STM32U5_HAL_SRC}/stm32u5xx_hal_i2c.c
    ${STM32U5_HAL_SRC}/stm32u5xx_hal_i2c_ex.c
)

stm32u575_apply_board(${PROJECT_NAME}.elf "-O2")

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    src
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
