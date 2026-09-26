# Vendored STM32U5 HAL / CMSIS

Trimmed subset of the official **STM32Cube_FW_U5** package
(**V1.9.0**), shared by every board in this repo:

```
drivers/
├── CMSIS/
│   ├── Include/                          CMSIS core headers (Cortex-M33)
│   └── Device/ST/STM32U5xx/Include/      STM32U575 device headers
└── STM32U5xx_HAL_Driver/
    ├── Inc/ (+ Legacy)                   HAL headers
    └── Src/                              HAL sources (the modules these projects use)
```

HAL modules vendored (`Src/`): `hal`, `adc(_ex)`, `cortex`, `dma(_ex)`,
`exti`, `flash(_ex)`, `gpio`, `i2c(_ex)`, `icache`, `pwr(_ex)`, `rcc(_ex)`,
`spi(_ex)`, `uart(_ex)`.

Builds use this by default — the board CMake helper sets
`STM32U5_HAL_ROOT` to `../../drivers`. The Cortex-M33 startup file
(`startup_stm32u575xx.s`) and `system_stm32u5xx.c` are **not** here; they live
with the board in `nucleo-u575/board/` (so a future board can ship its own).

To use the **full** official package instead (e.g. to pull in a HAL module
that is not vendored here), point `STM32U5_HAL_ROOT` at its `Drivers/` tree
when configuring:

```bash
cmake -G Ninja \
  -DSTM32U5_HAL_ROOT="C:/Users/user1/STM32Cube/Repository/STM32Cube_FW_U5_V1.9.0/Drivers" ..
```

The full package is STM32CubeMX → "Manage embedded software packages" →
STM32Cube MCU Package → "STM32Cube MCU Package for STM32U5 Series", installed
under `<STM32Cube>/Repository/STM32Cube_FW_U5_V1.9.0`.

All files remain under ST's original license (`LICENSE.txt` /
`LICENSE.md` in the respective folders).
