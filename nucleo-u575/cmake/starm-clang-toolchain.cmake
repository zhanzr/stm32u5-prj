# CMake toolchain file for ST's Arm Clang (starm-clang) + LLD.
#
# starm-clang is STMicroelectronics' LLVM/Clang toolchain shipped with
# STM32CubeIDE.  Unlike Keil armclang + GNU ld, starm-clang pairs with
# LLVM's own linker (LLD) which can consume LLVM bitcode, enabling full
# cross-TU LTO (-flto=full).
#
# C compilation: starm-clang (LLVM)
# Assembly:      GNU arm-none-eabi-gcc (for GAS-syntax startup_stm32u575xx.s)
# Linking:       starm-clang driver -> ld.lld (LLVM LLD)
#
# Selection (project CMakeLists):
#   set(STM32_TOOLCHAIN "starm-clang" CACHE STRING "...")
# or from the command line:
#   cmake -G Ninja -DSTM32_TOOLCHAIN=starm-clang ..
#
# Variables:
#   STARM_ROOT : ST Arm Clang tools root (contains bin/starm-clang.exe)

cmake_minimum_required(VERSION 3.13)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m33)

# CMake 3.30+ ARMClang support: suppress auto-added --cpu/--march flags.
if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

set(STARM_ROOT "D:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.llvm.win32_1.0.200.202603311046/tools" CACHE PATH
    "ST Arm Clang tools root (contains bin/starm-clang.exe)")

set(ARM_GCC_ROOT "D:/Arm/GNU Toolchain mingw-w64-x86_64-arm-none-eabi" CACHE PATH
    "GNU arm-none-eabi root (assembler, linker, newlib)")

# C compiler: starm-clang with LLVM's newlib sysroot. Locate the tools on
# PATH (add "<STARM_ROOT>/bin" and "<ARM_GCC_ROOT>/bin" to PATH before
# configuring).
find_program(CMAKE_C_COMPILER NAMES starm-clang.exe starm-clang REQUIRED)

# Derive STARM_ROOT from the resolved compiler so the LLVM newlib sysroot path
# used by the board helper is a valid absolute path.
get_filename_component(_STARM_BIN "${CMAKE_C_COMPILER}" DIRECTORY)
get_filename_component(_STARM_ROOT "${_STARM_BIN}" DIRECTORY)
set(STARM_ROOT "${_STARM_ROOT}" CACHE PATH "ST Arm Clang tools root (derived from PATH)" FORCE)

set(_STARM_SYSROOT "${STARM_ROOT}/lib/clang-runtimes/newlib")

# The newlib runtime is shipped as a sysroot with multilib variants. Compiling
# and linking need the sysroot (so <stdio.h>/... resolve) and the exact target
# the armv8m.main hard-float multilib declares (thumbv8m.main-st-none-eabihf,
# fpv5-sp-d16, unaligned access); otherwise clang picks no multilib and libc
# headers (<stdio.h>, ...) are not found. These are
# in CMAKE_C_FLAGS as well as the link line, so keep cpu/fpu here too.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --target=thumbv8m.main-st-none-eabihf -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16 -munaligned-access")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --sysroot=${_STARM_SYSROOT}")

# Assembly: GNU as (startup_stm32u575xx.s uses GAS syntax).
find_program(CMAKE_ASM_COMPILER NAMES arm-none-eabi-gcc.exe arm-none-eabi-gcc REQUIRED)

# Bare-metal target: compile-only sanity check during configure.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Object tools (ELF output from LLD link).
find_program(CMAKE_OBJCOPY NAMES starm-objcopy.exe starm-objcopy REQUIRED)
find_program(CMAKE_OBJDUMP NAMES starm-objdump.exe starm-objdump REQUIRED)
find_program(CMAKE_SIZE NAMES starm-size.exe starm-size REQUIRED)
find_program(CMAKE_AR NAMES starm-ar.exe starm-ar REQUIRED)

# Tells cmake/stm32u575_board.cmake to apply starm-clang-specific tweaks.
set(STM32_STARM_CLANG TRUE)
