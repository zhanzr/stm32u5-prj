# st7789s_md120_240x240_ft6336: ST7789S 1.2" 240x240 LCD (TK012F6 module)
# + FT6336 touch on the NUCLEO-U575ZI-Q @ 160 MHz, 3-wire serial (no D/C
# pin - 9-bit frames), HW SPI1 @ 40 MHz. Behavior follows the L4 nucleo-l4r5
# and f7-demo nucleo-f722 ports (identical module wiring).
# Wiring: SCL=PA5, SDA=PA7 (SPI1 AF5), CS=PA4; touch FT6336 on hardware
# I2C1 (SCL=PB8, SDA=PB9, AF4).
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

project(st7789s_md120_240x240_ft6336 C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Board include first: it resolves STM32U5_HAL_SRC (the vendored repo-root
# drivers/) which the source list below references.
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32u575_board.cmake)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/lcd.c
    src/interface.c
    src/touch.c
    src/lcd/lcd_fonts.c
    src/lcd/lcd_font_1608.c
    # SPI + I2C HAL (HW SPI1 bus path; FT6336 touch over hardware I2C1)
    ${STM32U5_HAL_SRC}/stm32u5xx_hal_spi.c
    ${STM32U5_HAL_SRC}/stm32u5xx_hal_spi_ex.c
    ${STM32U5_HAL_SRC}/stm32u5xx_hal_i2c.c
    ${STM32U5_HAL_SRC}/stm32u5xx_hal_i2c_ex.c
)

stm32u575_apply_board(${PROJECT_NAME}.elf "-O2")

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    src
    src/lcd
    src/blockwrite
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
