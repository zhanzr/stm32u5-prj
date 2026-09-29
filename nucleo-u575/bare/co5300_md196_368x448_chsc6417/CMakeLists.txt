# co5300_md196_368x448_chsc6417: CO5300 1.96" 368x448 LCM (TK0196M106 module)
# driven over plain single-lane SPI + CHSC6417 touch on hardware I2C1, on
# the NUCLEO-U575ZI-Q @ 160 MHz. Behavior copies
# nv3030b_md183_240x284_cst816d (same bus, same three wires); vendor
# references: the TK499 TK0196M106 single-lane example (display, vendor
# verbatim) and the vendor ESP32 QSPI example (CHSC6417 touch component).
# Wrapped command = CS frame with the 4-byte header 02 00 <cmd> 00 then the
# parameter/pixel bytes; the demo alternates SOFT (bit-bang) and HW (SPI1)
# on the same three wires so a difference is a difference in the SPI engine.
# Wiring: CS=PA4 SCK=PA5 MOSI=PA7 (SPI1 AF5; MISO not connected - the
# driver is write-only by design); touch CHSC6417 on I2C1 (SCL=PB8,
# SDA=PB9, AF4).
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

project(co5300_md196_368x448_chsc6417 C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Display brightness (DCS 51h). The datasheet defines it as a linear ratio
# DBV/256: 0xFF=100%, 0xC0=75%, 0xA0=62.5% (the vendor value), 0x80=50%,
# 0x60=37.5%, 0x40=25%, 0x20=12.5%, 0x00=0% (panel renders nothing).
# Default 0x01 = 0.78%, the lowest value that still shows an image - this
# panel is much brighter than the other modules on this board.
#   bash build.sh -DLCD_BRIGHTNESS=0x40
set(LCD_BRIGHTNESS "0x01" CACHE STRING "Display brightness, DCS 51h (0x01-0xFF)")

# Cold-boot bring-up passes: fill + LCD_Reinit cycles run before the
# pattern set. Lower = faster boot; raise to 3 if a cold power-on shows a
# blank panel (each pass gives a slow-starting panel another init).
#   bash build.sh -DLCD_BRINGUP_PASSES=3
set(LCD_BRINGUP_PASSES "2" CACHE STRING "Cold-boot fill+reinit cycles")

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
    src/asset_test1.c        # 64x64 RGB565 bring-up asset (generated)
    # SPI (display bus: soft bit-bang or SPI1) + I2C HAL (touch on I2C1)
    ${STM32U5_HAL_SRC}/stm32u5xx_hal_spi.c
    ${STM32U5_HAL_SRC}/stm32u5xx_hal_i2c.c
    ${STM32U5_HAL_SRC}/stm32u5xx_hal_i2c_ex.c
)

stm32u575_apply_board(${PROJECT_NAME}.elf "-O2")

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
    LCD_BRIGHTNESS=${LCD_BRIGHTNESS}
    LCD_BRINGUP_PASSES=${LCD_BRINGUP_PASSES}u
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    src
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
