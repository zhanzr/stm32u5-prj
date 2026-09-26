# blink_hello — LED chase + ADC internal channels (nucleo-u575)

Minimal "hello" demo for the **nucleo-u575** board (STM32U575ZIT6 @ 160 MHz,
hard-float). It chases the three board LEDs and once a second samples the
**ADC1 internal channels** and prints them on the **USART1** console.

## What it does

- **LEDs** (high-active): LD1 green **PC7**, LD2 blue **PB7**, LD3 red **PG2** —
  one LED lit at a time, advancing every 250 ms.
- **ADC1 internal channels** (12-bit, sampled once per second):

  | Channel | What it is |
  | ------- | ---------- |
  | VREFINT      | Internal reference voltage (~1.21 V) |
  | TEMPSENSOR   | Temperature sensor |
  | VBAT         | VBAT/**4** (internal 1/4 divider) |

  VREFINT is used to back out the actual supply voltage (`Vdda`), which then
  scales the temperature and VBAT readings. The temperature uses the factory
  calibration via `TEMPSENSOR_CAL1/2` (30 °C / 130 °C, taken at Vref+ = 3.0 V);
  the calibration constants come from `stm32u5xx_ll_adc.h`.

  > Note: the U5 divides VBAT by **4** internally (the L4 divided by 3), so the
  > VBAT reading is scaled by 4 — not 3.

Example output (once per second):

```
==== nucleo-u575 (STM32U575ZIT6) blink_hello @ 160 MHz ====
SYSCLK = 160000000 Hz (160 MHz)
FLASH_ACR latency = 4 (4 WS + ICACHE)
ADC1: VREFINT=1492 code, temp=943 code, VBAT=1030 code
     Vdda ~= 3317 mV, chip temp ~= 30 C, VBAT ~= 3333 mV
```

Exact values depend on the supply and die temperature; on a NUCLEO board with
no battery, `VBAT` is tied to `VDD`, so it reads ~3.3 V rather than 0.

## Build

```bash
cd nucleo-u575/bare/blink_hello

# GNU arm-none-eabi-gcc
bash build.sh

ninja -C build flash        # program via probe-rs (ST-Link SWD)
```

This project is GCC-only; the benchmark projects (`../dhry_160m`,
`../coremark_160m`) additionally support armclang / starm-clang.

## Console

**USART1** on **PA9 (TX) / PA10 (RX)**, AF7, **115200 8-N-1**, wired to the
ST-Link's virtual COM port. Read it on the `COMxx` the ST-Link enumerates.

```powershell
$sp = New-Object System.IO.Ports.SerialPort('COMxx',115200,[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One)
$sp.ReadTimeout = 15000; $sp.Open()
$sb = New-Object System.Text.StringBuilder
$deadline = [DateTime]::Now.AddSeconds(15)
while([DateTime]::Now -lt $deadline){ try { $b = $sp.ReadExisting(); if($b){ [void]$sb.Append($b) } else { Start-Sleep -Milliseconds 200 } } catch { break } }
$sp.Close(); $sb.ToString()
```
