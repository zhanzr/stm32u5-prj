# st7789s_md120_240x240_ft6336 - ST7789S 1.2" 240x240 LCD + FT6336 touch

Drives the **TK012F6** 1.2" **240x240** module - **ST7789S** LCD controller
plus **FT6336** capacitive touch - on the **nucleo-u575** board
(STM32U575ZIT6 @ 160 MHz). Ported from the L4 `nucleo-l4r5` and f7-demo
`nucleo-f722` projects (**identical module wiring**); those ports in turn
followed the f4-demo `jd9851_md140_240x240_cst816d` port: hardware SPI1, the
full pattern set with timed solid fills, live FPS counter, and touch printout
on the serial port.

The pin wiring is **identical** on every port (both are Nucleo-144 boards);
only the MCU, clock and peripheral clock dividers change.

## Driving the ST7789S (3-wire 9-bit serial)

This module connector has **no D/C pin**: the panel is strapped for
3-wire serial - every byte is a **9-bit frame** (D/C bit clocked first,
then 8 data bits, MSB first), the same protocol the ili9163c/ili9341v
projects use. The U5 SPI only produces 8/16-bit frames, so the HW path
transmits **16-bit words packed with the 9-bit-frame bitstream**
(transparent to the panel), SPI mode 3.

- **Init sequence** (vendor-verbatim): sleep out (120 ms), MADCTL 0x00,
  COLMOD, porch (0xB2 = 0x1F/0x1F/0x00/0x33/0x33), gate levels (0xB7 =
  0x12: VGH 12.54 V / VGL -8.23 V), VCOM (0xBB = 0x66), power (0xC0 =
  0x2C, 0xC2 = 0x01, 0xC3 = 0x15 = 4.6 V, 0xC4 = 0x20 = 0 V), frame rate
  (0xC6 = 0x13), gate/source drivers (0xD0 = 0xA4A1, 0xD6 = 0xA1:
  sleep-in gate to GND), gamma (0xE0/0xE1), COLMOD 65k (0x3A = 0x05),
  MADCTL 0x00, INVOFF, display on.
- **Geometry**: 240x240, windows at COL_Pre = 0, ROW_Pre = 0 (full range
  addressed directly), MADCTL 0x00.
- **No reset pin** on this module: the vendor sequence settles CS (high,
  then low) for 100 ms before the init commands. `LCD_Reinit` re-runs
  display-off + the full sequence (the sleep out inside provides the
  reset).
- **No backlight pin** in this wiring: the module's backlight is
  hardwired on.
- **Clock**: APB2 = 160 MHz, prescaler /4 = **40 MHz SCK** — comfortably
  under the ST7789S serial-write limit (~62.5 MHz) and the f7 port's
  54 MHz. `LCD_SPI1_PRESC` selects /8 = 20 MHz or /16 = 10 MHz. The SCL/SDA
  pins use `GPIO_SPEED_FREQ_HIGH` (VERY_HIGH edges ring on flying wires - the
  failure mode that hit the ili9163c bring-up on the f746).

## Touch: FT6336 over hardware I2C1

PB8/PB9 are the I2C1 AF4 pins, so the touch bus is the **hardware
peripheral**: 100 kHz standard mode, TIMINGR computed for PCLK1 = 160 MHz
(PRESC=15, SCLDEL=4, SDADEL=2, SCLH=SCLL=49 → `0xF4203131`). 7-bit slave
address **0x38**.

Reading 8 bytes from register 0x00 gives: `0x02` TD_STATUS (1 = one touch
point), `0x03` P1_XH ([3:0] X high), `0x04` P1_XL, `0x05` P1_YH ([3:0] Y
high), `0x06` P1_YL. Touch poll runs during every wait; the console
prints `[TOUCH] down X=.. Y=..` on touch and `[TOUCH] release` on lift.

## Wiring

Identical to the L4 `nucleo-l4r5` and f7-demo `nucleo-f722` ports (all are
Nucleo-144 boards):

| LCD pin | MCU pin | Feature |
| ------- | ------- | ------- |
| SCL | PA5 | SPI clock (HW SPI1_SCK, AF5) |
| SDA/MOSI | PA7 | SPI data out (HW SPI1_MOSI, AF5) |
| CS  | **PA4** | Chip select (GPIO software CS) |
| RST | - | **no reset pin** on this module (vendor settles CS) |
| BL  | - | **not wired** (module backlight is on whenever powered) |

| Touch pin | MCU pin | Feature |
| --------- | ------- | ------- |
| TOUCH_SCL | **PB8** | I2C1_SCL (AF4, hardware) |
| TOUCH_SDA | **PB9** | I2C1_SDA (AF4, hardware) |

## What it does

The demo loops forever on the HW SPI1 bus @ 40 MHz, running the **full
pattern set** - TEST_STAND (frame / 16-level gray / bands / solid colors,
timed), info pages (normal + inverted, with the solid-fill durations),
HSV gradient sweep, LED test (the three on-board LEDs) - with a live FPS
counter, plus touch printout on the serial port.

Measured on hardware: **80 ms** per 240x240 solid fill at 40 MHz SCK
(57,600 px). As on the L4/f7 ports this is dominated by the STM32 SPI HAL's
per-frame overhead (the HAL raises `TXE` only when the TX FIFO is empty), not
the wire time - the L4 port measured 105 ms at a *higher* 60 MHz for the same
reason. If a module/wiring proves marginal, drop to /8 via `LCD_SPI1_PRESC`.

## Build / flash / console

```bash
bash build.sh                    # configure + build into ./build (GNU arm-none-eabi-gcc)
ninja -C build flash             # probe-rs download + reset over ST-Link SWD
```

Console is **USART1** (PA9/PA10, ST-Link VCP, 115200 8-N-1); a banner
prints once at boot and the pattern phases log as they run, looping
forever:

```
==== nucleo-u575 (STM32U575ZIT6) st7789s_md120_240x240_ft6336 @ 160 MHz ====
ST7789S 1.2" 240x240 (3-wire 9-bit serial, MADCTL 0x00):
SCL=PA5 SDA=PA7 CS=PA4; no DC/RST/BL pin (in-protocol DC, backlight hardwired)
TOUCH: FT6336 I2C1 SCL=PB8 SDA=PB9
[LCD] phase: HARDWARE banner
[LCD] running patterns on HARDWARE SPI1 @ 40 MHz
[LCD] phase: TEST_STAND
[LCD] solid fills (ms): RED=80 GREEN=80 BLUE=80 WHITE=80 BLACK=80
[LCD] phase: info
[LCD] info: compiler=GCC 15.3.1 build=Sep 27 2026 07:10:23
[LCD] info: freq=160 MHz drive=HW SPI1
[LCD] info: UID=001F004A5243501220303730
[LCD] phase: info (inverted colors)
[LCD] phase: gradient
[LCD] phase: LED test
[LCD] LED ON
[LCD] LED OFF
[TOUCH] down X=.. Y=.. (240-Y=..)
[TOUCH] release
```

> **NOTE**: power-cycle the MODULE (unplug/replug its power, not just
> NRST) before judging a fix - the panel has no reset pin, so a latched
> bad state survives MCU resets.

## Files

- `src/main.c` - HW pattern loop + FT6336 touch printout
- `src/lcd.c` / `lcd.h` - ST7789S init + 240x240 geometry + runtime
  drawing window + drawing API + `LCD_Reinit`
- `src/interface.c` / `interface.h` - 3-wire 9-bit bus primitives (HW
  SPI1 @ 40 MHz, packed 16-bit words)
- `src/touch.c` / `touch.h` - FT6336 over hardware I2C1
- `src/lcd/lcd_fonts.c` / `lcd_fonts.h` - ASCII 6x12 font
- `src/lcd/lcd_font_1608.c` / `lcd_font_1608.h` - ASCII 8x16 banner font
- `src/blockwrite/blockwrite.h` - pixel-window helper
