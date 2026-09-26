# LTO and Dhrystone: why the inflated score is an artifact

**Short version:** Dhrystone is trivially vulnerable to whole-program
optimization. With GCC `-flto` the compiler sees the entire benchmark at once,
hoists the (loop-invariant) work out of the timed loop, and the score jumps
dramatically. The result still *looks* correct (the hoisted code still runs
once, so the final values match), which is exactly why this number must never
be quoted. This is a **known, documented weakness of Dhrystone itself**, not a
bug in our toolchain or setup.

## What Dhrystone actually measures

Dhrystone 2.1 is a single `for` loop (here 10,000,000 runs) calling
`Proc1`/`Proc2`/`Proc3`/`Func1`…`Func3` on a small set of globals. Two
structural facts make it optimizable:

- The loop body has a lot of **loop-invariant work** — string copies of
  constant-length, aligned strings, fixed arithmetic, and assignments that
  depend only on values that never change across iterations.
- The final value of every global is the **same after every iteration** (they
  are reset or re-derived each pass), so a compiler can compute the result of
  one iteration and conclude the rest are redundant.

The benchmark was published in 1984/1988, before compilers could see across
translation units, so its "verification" (compare final globals against
expected constants) only catches blatant dead-code elimination — not
loop-invariant hoisting, which is legal and preserves the final values.

## What LTO does here

Per-object `-Ofast` compiles each `.c` file alone: `dhry_1.c` cannot see the
repeat loop in `main.c`, so it must keep every statement. GCC `-flto` exports
GIMPLE instead of machine code, and the link-time plugin re-runs optimization
on the **whole program**. It then:

1. inlines `Proc*`/`Func*` into the timed loop,
2. proves much of the body is loop-invariant across the fixed 10,000,000
   iterations, and
3. **hoists it out of the loop** (loop-invariant code motion) and/or deletes
   redundant recomputation.

The timed region shrinks to a skeleton that re-checks a couple of values per
iteration. The remaining (hoisted) code still executes — once — so the printed
`Int_Glob`, `Arr_2_Glob`, etc. are still correct and the built-in check passes.

## Reference measurements (nucleo-l4r5, STM32L4R5ZIT6 @ 120 MHz, hard-float)

The numbers below are from the sibling **L4** port of this project, which is
where the artifact was first characterised. The same behaviour applies on the
U575 at 160 MHz; measure it there with the recipe further down.

| Build               | Flags                                      | µs/run | Dhrystones/s | DMIPS/MHz |
| ------------------- | ------------------------------------------ | ------ | ------------ | --------- |
| GCC 15.3.1          | `-Ofast -ffp-contract=fast -funroll-loops` | 4.143  | 241,365      | 1.145     |
| GCC 15.3.1 + LTO    | above `+ -flto`                            | 1.901  | 526,177      | 2.496 ⚠   |
| armclang 6.24 (AC6) | `-Ofast -ffp-contract=fast -funroll-loops` | 3.576  | 279,642      | 1.326     |

- Non-LTO GCC and armclang agree with each other within ~15 % — consistent,
  meaningful numbers.
- The LTO build ran **2.18× faster per iteration** with identical final values.
  Per-run time dropped from 4.143 µs to 1.901 µs; the "extra" 2.242 µs of work
  was simply moved out of the timed region.
- This is the same mechanism reported elsewhere: a public aarch64 example shows
  Dhrystone inflating from 5.2 M to 19.5 M Dhrystones/s (~3.7×) with `-flto` and
  a 3.5× drop in executed instructions.

## It's a known issue

Not a local anomaly — the Dhrystone-vs-LTO problem is documented by the
benchmark's own ecosystem:

- **EEMBC** (CoreMark authors): *"major portions of Dhrystone actually expose
  the compiler's ability to optimize the workload rather than the capabilities
  of an MCU."* CoreMark was designed to fix exactly this: every operation
  depends on a per-run CRC, so the work cannot be hoisted.
- Every reputable Dhrystone comparison therefore states its compiler and
  flags, and does **not** enable LTO for the timed loop.

## How to reproduce (and how to avoid quoting it)

```bash
cd nucleo-u575/bare/dhry_160m
BUILD_DIR=build-gcc-lto bash build.sh -DSTM32_LTO=ON     # the artifact
ninja -C build-gcc-lto flash                             # inflated score
```

The non-LTO build (`bash build.sh`) is the number to quote. `syscalls.c` is
compiled `-fno-lto` even under `-DSTM32_LTO=ON` (a newlib retarget fix, see
`../../cmake/stm32u575_board.cmake`), so it does not affect the timed loop.
