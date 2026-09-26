# STM32U5 projects (multi-board)

Bare-metal firmware projects and tooling for multiple **STM32U5-series**
boards/chips. Each board lives in its own folder with a self-contained
`board/` (clock, LEDs, console, startup, linker script), `cmake/` (toolchain +
board helpers) and `bare/` (projects). Board-level docs — hardware, clock tree,
build/flash/console — live in each board folder's README.

## Boards

| Board         | What it is                                          |
| ------------- | --------------------------------------------------- |
| `nucleo-u575/` | NUCLEO-U575ZI-Q (STM32U575ZIT6) @ 160 MHz, MSI 4 MHz, 3 LEDs (PC7/PB7/PG2), USART1 console @ 115200 (see its README) |

The `-<chip>` suffix in board folder names keeps it a multi-board/**multi-chip**
repo: e.g. a `nucleo-u5a5` board would sit next to `nucleo-u575`, a
`custom-u585` board would sit next to the U575 one, and boards with the same
MCU but a different pinout are siblings.

## Vendored HAL / CMSIS

The STM32U5 **HAL driver + CMSIS** are vendored in the repo root `drivers/`
(trimmed subset of the official `STM32Cube_FW_U5` V1.9.0 package):

```
drivers/
├── CMSIS/
│   ├── Include/                        CMSIS core headers (Cortex-M33)
│   └── Device/ST/STM32U5xx/Include/    STM32U5 device headers
└── STM32U5xx_HAL_Driver/
    ├── Inc/ (+ Legacy)                 HAL headers
    └── Src/ (used HAL modules)         HAL sources
```

Builds use this by default (`-DSTM32U5_HAL_ROOT` defaults to `../../drivers`).
The trimmed tree covers everything these projects compile; it does **not**
include the BSP, middleware, projects or docs.

To use the **full** official package instead (e.g. to pull in something not
vendored), point `STM32U5_HAL_ROOT` at its `Drivers/` tree when configuring:

```bash
cmake -G Ninja -DSTM32U5_HAL_ROOT="C:/Users/user1/STM32Cube/Repository/STM32Cube_FW_U5_V1.9.0/Drivers" ..
```

## Toolchain / environment

* GNU arm-none-eabi-gcc (default) or Keil AC6 armclang (`-DSTM32_TOOLCHAIN=armclang`); ST Arm clang (starm-clang) where supported. The non-GNU compilers are located on `PATH`, so add their `bin` dirs before configuring.
* CMake + Ninja (Pico-style; MSYS2 mingw64 `build.sh` adds them to `PATH`).
* probe-rs (SWD flashing) + OpenOCD (alternative).

## Build configuration

Configure and build each project from its own directory (`<board>/bare/<project>`).
No absolute paths are needed — the build directory is a relative subfolder of
the project.

```bash
cd nucleo-u575/bare/dhry_160m

# default: gcc + the project's hard-coded optimization level
bash build.sh          # == cmake -G Ninja -S . -B build && ninja -C build
ninja -C build flash   # program via probe-rs (ST-Link SWD)

# other toolchains: use one build dir per toolchain
# (CMAKE_TOOLCHAIN_FILE is cached at configure time)
BUILD_DIR=build-armclang bash build.sh -DSTM32_TOOLCHAIN=armclang
BUILD_DIR=build-starm-clang bash build.sh -DSTM32_TOOLCHAIN=starm-clang
```

> **Windows note.** `build.sh` selects the **native mingw64** CMake + Ninja on
> MSYS2. The MSYS2 (`/usr/bin`) CMake writes POSIX paths (e.g.
> `/usr/bin/cmake.exe`) into `build.ninja`, which only MSYS2-aware Ninja can
> execute — a native Ninja (MINGW64 shell or a standalone install) then fails
> with `CreateProcess failed: The system cannot find the file specified.`
> Native CMake emits plain Windows paths, so the build tree works from any
> shell. Keep CMake and Ninja from the **same** environment, and prefer
> `bash build.sh` over invoking `cmake`/`ninja` by hand.

**Optimization levels** are passed to the board-apply CMake function in each
project's `CMakeLists.txt`:

* **General projects** need nothing on the command line — their optimization is
  hard-coded at a sane level (`-O1` for simple demos, `-O2`/`-O3` for
  peripheral-heavy apps).
* **Benchmark projects** (`dhry_*`, `coremark_*`) default to aggressive flags
  and let you override them at configure time with `-DBENCH_OPT="..."` (plus
  `-DBENCH_OPT_C="..."` for C-only options). The fastest per-toolchain
  settings are documented in each benchmark's README. Note that armclang
  `-Omax` enables LTO (LLVM bitcode), which only Keil's armlink can link — not
  the GNU ld used here.
