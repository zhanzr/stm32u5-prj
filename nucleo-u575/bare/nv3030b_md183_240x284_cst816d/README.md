# nv3030b_md183_240x284_cst816d - NV3030B 1.83" 240x284 LCM over plain SPI + CST816D touch

Drives the **TK018F3716** 1.83" **240x284** module - **NV3030B** LCD
controller plus **CST816D** capacitive touch - on the **nucleo-u575** board
(STM32U575ZIT6 @ 160 MHz), over **plain single-lane SPI**. Ported from the
ch32v307 reference `nv3030b_md183_240x284_cst816d` (same module, vendor
verbatim registers).

The project originally drove the panel over **OCTOSPI1 (QuadSPI)** with a
"quad -> dual -> single" data-lane loop. That path was **dropped**: the
STM32 HAL's `HAL_OSPI_Transmit()` writes **one byte per `DR` write** with a
FIFO-flag poll each time, so a full-screen fill cost ~8 M CPU operations and
measured **the same 46 ms whether the data phase was 1 line or 4 lines** -
the bus was never the bottleneck, so a wide data phase could not be
exercised at all. Plain SPI on three wires is simpler and measurable.

## Protocol (wrapped command)

Every transaction is ONE chip-select frame:

```
CS low
02 00 <cmd> 00          <- 4-byte header (opcode, 0, command, 0)
... parameter / pixel bytes ...
CS high
```

- The `02` prefix **is** the protocol header (the vendor's `WriteComm` sends
  exactly `02 00 <cmd> 00`); it is the 1-data-line transfer opcode, not an
  OCTOSPI instruction phase.
- Keeping the pixel bytes in the **same CS frame** as their command is what
  lets a row be streamed without re-sending the header.
- SPI mode 3, MSB first (the vendor configuration, proven on this module).
- Write-only: the driver never reads the panel and MISO is not connected.
  The wrapped protocol needs no readback (no status polling, no RAM read).
- Pixel bursts are row-chunked: the window is set once (2Ah/2Bh), the first
  row goes out as 2Ch, further rows as 3Ch (write memory continue).

## The Soft <-> Hard SPI loop

Both transports drive the **same three wires** with the same framing, so a
difference between the passes is a difference in the SPI engine itself:

- **SOFT** - bit-banged SPI on plain GPIOs, the vendor TK499 sequence bit
  for bit: SCK falls, data is set, SCK rises (the panel samples on the
  rising edge). SCK is left high (mode-3 idle). Rate set by
  `LCD_SOFT_SPI_DIV` (default 4) and measured at init.
- **HW** - the **SPI1** peripheral, 8-bit frames, mode 3, `LCD_SPI_PRESC`
  (default `/4` = 40 MHz; SPI1 is on APB2). CS stays a software GPIO.

Each pass re-initialises the panel from scratch (so neither inherits the
other's panel state) and runs the full pattern set: checkerboard stress,
TEST_STAND vendor screens with timed solid fills, info pages (normal +
inverted), HSV gradient, LED test - with a live FPS counter and touch
printout throughout.

> Note on the HW path: `HAL_SPI_Transmit()` disables the peripheral at the
> end of every call, and the U5's new-generation SPI IP releases the SCK/
> MOSI alternate functions on disable unless `MasterKeepIOState` is enabled.
> With CS already low that edge is latched by the panel as an extra bit and
> the whole stream shifts by one (all-white display). `interface.c` sets
> `SPI_MASTER_KEEP_IO_STATE_ENABLE` and keeps the header + data inside one
> CS frame across the two transmit calls.

## Wiring

| Module pin | MCU pin | Feature |
| ---------- | ------- | ------- |
| QSPI_CS   | **PA4** | GPIO output (software CS, both transports) |
| QSPI_CLK  | **PA5** | SPI1_SCK - also the soft bit-bang SCK |
| QSPI_IO0  | **PA7** | SPI1_MOSI - also the soft bit-bang MOSI |
| MISO      | -       | not connected (write-only driver, by design) |

Touch bus (unchanged):

| Touch pin | MCU pin | Feature |
| --------- | ------- | ------- |
| TOUCH_SCL | **PB8** | I2C1_SCL (AF4, hardware, 100 kHz) |
| TOUCH_SDA | **PB9** | I2C1_SDA (AF4, hardware) |

Common GND between the module and the board is required.

## Timing / knobs

- Soft SCK is measured at init and printed (div 4 measured 1882 kHz on this
  board). Raise `LCD_SOFT_SPI_DIV` to slow it down.
- Hardware SCK = PCLK2 / prescaler = 160 MHz / 4 = **40 MHz**. The NV3030B
  allows up to 64.9 MHz on SCL (15.4 ns write cycle) at 3.3 V, but flying
  wires will not - drop to `/8` (20 MHz) if the checkerboard shows noise.
- GPIO slew HIGH (VERY_HIGH edges ring on flying wires - see the st7789s
  port notes).
- No reset pin: **power-cycle the MODULE** (not just NRST) before judging a
  fix - a latched bad state survives MCU resets.
- Cold boot: the driver waits `LCD_POWER_SETTLE_MS` (500 ms) before the
  first clock; the checkerboard stress pattern right after init is the
  "did the init land" test.
- `QSPI_EnableBlock()` sends the `DEh/DFh/CEh/D8h` unlock block from the
  vendor ESP32 example. The **single-lane** vendor examples for this module
  do not carry it and it is unverified here - it is isolated in that one
  function so it is easy to disable.

## Build / flash / console

```bash
bash build.sh                    # configure + build into ./build (GNU arm-none-eabi-gcc)
ninja -C build flash             # probe-rs download + reset over ST-Link SWD
```

Console is **USART1** (PA9/PA10, ST-Link VCP, 115200 8-N-1):

```
==== nucleo-u575 (STM32U575ZIT6) nv3030b_md183_240x284_cst816d @ 160 MHz ====
NV3030B 1.83" 240x284 (wrapped-command SPI, MADCTL 0x08):
CS=PA4 SCK=PA5 MOSI=PA7 (MISO not connected - write-only)
loop: SOFT (bit-bang) <-> HW (SPI1), same 3 wires
TOUCH: CST816D I2C1 SCL=PB8 SDA=PB9
[SPI] soft SPI (bit-banged) SCK=1882 kHz, div=4
[TOUCH] self-test: ACK (chip present)

==== bus: SOFT bit-banged SPI ====
[LCD] SCK = 1.8 MHz
[LCD] solid fills (ms): RED=581 GREEN=581 BLUE=581 WHITE=576 BLACK=583

==== bus: HW SPI1 peripheral ====
[LCD] SCK = 40 MHz
[LCD] solid fills (ms): RED=.. GREEN=.. BLUE=.. WHITE=.. BLACK=..
```

## Files

- `src/main.c` - Soft/Hard SPI loop + pattern set + touch printout
- `src/interface.c` / `interface.h` - the two transports behind one byte API
  (`QSPI_Write` / `QSPI_WritePixel` share one CS frame per transaction)
- `src/lcd.c` / `lcd.h` - NV3030B init (vendor verbatim) + 240x284 geometry
  + drawing API + `LCD_Reinit`
- `src/touch.c` / `touch.h` - CST816D over hardware I2C1
- `src/lcd_fonts.c` / `lcd_font_1608.c` - ASCII 6x12 / 8x16 fonts
  (verbatim from the reference)
