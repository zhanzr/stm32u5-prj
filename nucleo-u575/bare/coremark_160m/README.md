# CoreMark 1.0.1 @ 160 MHz — nucleo-u575 (STM32U575ZIT6)

CoreMark 1.0.1 (EEMBC, `coremark_1_0_1/`), **10,000 iterations**, on the
**nucleo-u575** board (STM32U575ZIT6) at **160 MHz** (MSI 4 MHz → PLL M=1
N=80 R=2 → SYSCLK, hard-float). Compiler-agnostic: the same sources build with
**GNU arm-none-eabi-gcc**, **Keil Arm Compiler 6 (armclang)** or **ST Arm
clang** (starm-clang), selected at configure time. The CoreMark port uses the
HAL SysTick 1 kHz tick from `src/core_portme.c`, so it works identically on all
three compilers.

## Results

Measured on hardware at 160 MHz (hard-float): capture the console while the
chip runs the benchmark (it re-runs every ~30 s), and take the last complete
`Iterations/Sec` line.

| Toolchain           | Flags                                          | iterations/s | Time (s) |
| ------------------- | ---------------------------------------------- | ------------ | -------- |
| GCC 15.3.1          | `-Ofast -ffp-contract=fast -funroll-all-loops` | 504.39       | 19.83    |
| GCC 15.3.1 + LTO    | above `+ -flto`                                | 479.62       | 20.85    |
| ARMCLANG (Keil AC6) | `-Ofast -ffp-contract=fast -funroll-all-loops` | 540.02       | 18.52    |
| ARMCLANG (Keil AC6) | `-Omax -fno-lto`                               | **633.87**   | **15.78** |
| ST Arm clang 21.1.1 | `-Ofast -ffp-contract=fast`                    | 476.24       | 21.00    |

All runs share the same CRC (`crcfinal 0x988c`) and report
`Correct operation validated`. Note CoreMark's per-run CRC forces the work to
execute, so **LTO does not inflate it** the way it cheats Dhrystone — see
`../dhry_160m/LTO_on_dhrystone.md`.

## Most aggressive flags

Highest measured score per toolchain (see Results):

- **ARMCLANG (Keil AC6): `-Omax -fno-lto`** — 633.87 it/s, the fastest measured
  configuration overall (1.26× the armclang `-Ofast` score). Bare `-Omax` makes
  armclang emit LLVM **LTO** objects that GNU ld cannot link, hence `-fno-lto`.
  Pass it as a **C-only** flag (`BENCH_OPT_C`) so it stays off the asm/link
  steps, with `BENCH_OPT` cleared.
- **GCC:** `-Ofast -ffp-contract=fast -funroll-all-loops` (already the default)
  → 504.39 it/s; adding `-DSTM32_LTO=ON` does **not** help here (479.62 it/s).
- **ST Arm clang:** 476.24 it/s at its `-Ofast -ffp-contract=fast` default;
  `-funroll-all-loops` is not supported by clang.

```bash
BUILD_DIR=build-armclang-omax bash build.sh -DSTM32_TOOLCHAIN=armclang '-DBENCH_OPT=' '-DBENCH_OPT_C=-Omax -fno-lto'
BUILD_DIR=build-gcc-lto       bash build.sh -DSTM32_LTO=ON
BUILD_DIR=build-starm-clang   bash build.sh -DSTM32_TOOLCHAIN=starm-clang
```

## Build

Requires the CMake/Ninja environment from the board-level `../../README.md`.

```bash
cd nucleo-u575/bare/coremark_160m

# GNU gcc (default)
bash build.sh
ninja -C build flash          # programs the board via probe-rs / ST-Link (SWD)

# armclang (optional) — put Keil's bin dir on PATH so CMake can find armclang
export PATH="/d/Keil_v5/ARM/ARMCLANG/bin:$PATH"
BUILD_DIR=build-armclang bash build.sh -DSTM32_TOOLCHAIN=armclang

# armclang at -Omax (BENCH_OPT_C is applied to the C files only, and -fno-lto
# is required because -Omax would otherwise emit LTO objects GNU ld cannot link)
BUILD_DIR=build-armclang-omax bash build.sh -DSTM32_TOOLCHAIN=armclang \
    '-DBENCH_OPT=' '-DBENCH_OPT_C=-Omax -fno-lto'

# GNU gcc + LTO
BUILD_DIR=build-gcc-lto bash build.sh -DSTM32_LTO=ON

# ST Arm clang (starm-clang) + LLD — add its bin dir to PATH first
export PATH="/d/ST/STM32CubeIDE_2.1.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.llvm.win32_1.0.200.202603311046/tools/bin:$PATH"
BUILD_DIR=build-starm-clang bash build.sh -DSTM32_TOOLCHAIN=starm-clang
BUILD_DIR=build-starm-lto   bash build.sh -DSTM32_TOOLCHAIN=starm-clang -DSTM32_LTO=ON
```

`build.sh` puts the native mingw64 CMake/Ninja on `PATH`; for armclang /
starm-clang, add their `bin` dirs to `PATH` first (the toolchain files locate
them with `find_program`). See the Windows note in the root `README.md`.

Use a separate build dir per toolchain (`build/`, `build-gcc-lto/`,
`build-armclang/`, `build-starm-clang/`) because `CMAKE_TOOLCHAIN_FILE` is
cached after configure.

## Console

**USART1** on **PA9 (TX) / PA10 (RX)**, AF7, **115200 8-N-1**, wired to the
ST-Link's virtual COM port (see the board-level `../../README.md` for a capture
recipe).
