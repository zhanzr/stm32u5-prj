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

### Cold-boot bring-up (ported from the reference)

Before the patterns, each pass runs a **bring-up-by-drawing** loop
(`panel_bringup()`): tile the screen with the 64x64 RGB565 asset
(`asset_test1`), `LCD_Reinit()`, repeat - three times, then one final fill
with the last init in effect.

How long a cold-booted panel needs before it accepts commands varies run
to run and cannot be measured (the module is write-only by design), so
this deliberately avoids sleeping or counting retries. The drawing **is**
the wait and doubles as the test: the moment the artwork appears, the
panel is live. Interleaving fills with `LCD_Reinit()` gives a
slow-starting panel several chances to catch an init.

Tiles are whole and the grid is centred, so the panel's rounded corners
never clip one; the range stays inside `INFO_TOP..anim_h()` to clear the
rows above and the FPS band below.

### The bring-up asset (`asset_test1`)

`src/asset_test1.c` / `.h` are **generated** (4096 `uint16_t` = 64x64,
RGB565 row-major, alpha composited over black). Do not edit by hand -
regenerate with the checked-in tool:

```bash
python tools/png_to_rgb565.py <input.png> src/asset_test1.c asset_test1
```

`tools/png_to_rgb565.py` is pure standard library (zlib only), so it runs
in a plain Python install with no Pillow/numpy. The asset is byte-identical
to the one used by the ch32v307 reference and the tricore ports.

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
  vendor ESP32 example. It is **OFF by default**: the block is not in the
  NV3030B datasheet and not in the single-lane vendor examples, so with it
  off the init sequence is byte-for-byte identical to the proven ch32v307
  reference. Enable it to test the hypothesis that this glass needs the
  QSPI register region unlocked:

  ```bash
  bash build.sh -DLCD_INIT_ENABLE_BLOCK=1
  ```

  Use a separate build dir to keep both variants available and A/B them:

  ```bash
  BUILD_DIR=build-eb bash build.sh -DLCD_INIT_ENABLE_BLOCK=1
  ```

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
init: vendor SPI sequence only (reference-exact)
TOUCH: CST816D I2C1 SCL=PB8 SDA=PB9
[SPI] soft SPI (bit-banged) SCK=1860 kHz, div=4
[TOUCH] self-test: ACK (chip present)

==== bus: SOFT bit-banged SPI ====
[LCD] SCK = 1.8 MHz
[LCD] bring-up 1/3: draw
[LCD] bring-up: Reinit
[LCD] bring-up 2/3: draw
[LCD] bring-up: Reinit
[LCD] bring-up 3/3: draw
[LCD] bring-up: Reinit
[LCD] bring-up: done
[LCD] solid fills (ms): RED=588 GREEN=588 BLUE=588 WHITE=588 BLACK=588

==== bus: HW SPI1 peripheral ====
[LCD] SCK = 40 MHz
[LCD] bring-up 1/3: draw
...
[LCD] solid fills (ms): RED=37 GREEN=37 BLUE=37 WHITE=37 BLACK=37
```

## Files

- `src/main.c` - Soft/Hard SPI loop + bring-up + pattern set + touch printout
- `src/interface.c` / `interface.h` - the two transports behind one byte API
  (`QSPI_Write` / `QSPI_WritePixel` share one CS frame per transaction)
- `src/asset_test1.c` / `.h` - generated 64x64 RGB565 bring-up asset
- `tools/png_to_rgb565.py` - regenerates the asset from a PNG
- `src/lcd.c` / `lcd.h` - NV3030B init (vendor verbatim) + 240x284 geometry
  + drawing API + `LCD_Reinit`
- `src/touch.c` / `touch.h` - CST816D over hardware I2C1
- `src/lcd_fonts.c` / `lcd_font_1608.c` - ASCII 6x12 / 8x16 fonts
  (verbatim from the reference)
