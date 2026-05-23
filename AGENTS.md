# AGENTS.md

This file explains how to work on this MSPM0G3507 firmware project safely.
The project uses CMake + GCC + OpenOCD, not a CCS project layout, but the
SysConfig and DriverLib rules still matter.

## Project Shape

- `core/` contains the MSPM0 SDK surface used by this project: startup code,
  linker script, SysConfig output, DriverLib, CMSIS, FreeRTOS, and syscall glue.
- `core/trobot.syscfg` is the source of truth for clocks, pins, peripherals,
  DMA channels, interrupts, and generated initialization names.
- `bsp/` contains board support code for UART, SPI, LCD, flash, time, GPIO, and
  low-level helpers.
- `components/` contains optional reusable modules. `components/utils` is a git
  submodule and is required by the current application.
- `app/` contains the firmware entry point and application tasks.
- `*.cfg` files at the repository root select the OpenOCD probe interface.

## Build And Flash

Use an ARM embedded GCC toolchain.

```powershell
cmake -S . -B cmake-build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug
```

The normal output files are:

- `cmake-build-debug/trobot.elf`
- `cmake-build-debug/trobot.hex`
- `cmake-build-debug/trobot.bin`
- `cmake-build-debug/trobot.map`

This project needs an OpenOCD build with TI MSPM0 support. Before flashing,
check which OpenOCD executable is active:

```powershell
where openocd
openocd --version
```

Use the first `where openocd` result to understand which executable will run.
Then verify that it can resolve this project's MSPM0 target scripts. A quick
script-resolution check that should not initialize the adapter is:

```powershell
openocd -f daplink.cfg -c "shutdown"
```

With a DAPLink/CMSIS-DAP probe connected:

```powershell
cmake --build cmake-build-debug --target flash_and_verify
```

For other probes, use the matching root config manually:

```powershell
openocd -f xds110.cfg -c "init" -c "reset halt" -c "program cmake-build-debug/trobot.elf verify reset exit"
openocd -f stlink.cfg -c "init" -c "reset halt" -c "program cmake-build-debug/trobot.elf verify reset exit"
```

If OpenOCD reports that it cannot find `target/ti_mspm0.cfg`, the wrong OpenOCD
is being used or its script search path is wrong. If it reports that it cannot
find a matching CMSIS-DAP device, the MSPM0 scripts were found and the remaining
problem is probe/USB/driver/hardware related.

## SysConfig Rules

Treat `core/trobot.syscfg` as the peripheral configuration source, but do not
modify it by default. Only edit `core/trobot.syscfg` when the user explicitly
asks for a change that requires SysConfig, such as pins, clocks, DMA,
UART/SPI/I2C/ADC/timer setup, or interrupt ownership.

When a user-requested change does require SysConfig:

1. Edit `core/trobot.syscfg` with TI SysConfig when possible.
2. Preserve the metadata comments at the top of the file, including `@cliArgs`,
   `@v2CliArgs`, `@versions`, device, package, and SDK product.
3. Regenerate the SysConfig outputs into `core/`.
4. Re-read `core/ti_msp_dl_config.h` before using generated names in code.
5. Build the project.

Do not guess generated names. Use the local macros and function spellings from
`core/ti_msp_dl_config.h`, such as `SYSCFG_DL_init()`, `UART_DEBUG_INST`,
`DMA_UART0_TX_CHAN_ID`, `SPI1_INST`, and `GPIO_BOARD_LED_PIN`.

Be careful with these generated or SysConfig-owned files:

- `core/ti_msp_dl_config.c`
- `core/ti_msp_dl_config.h`
- `core/device_linker.lds`

It is acceptable for this repository to track those files because the CMake
build consumes them directly. However, avoid manual edits unless the change is
intentional, reviewed, and cannot reasonably be represented in SysConfig.

## Coding Rules

- Prefer existing BSP APIs over directly touching DriverLib from `app/`.
- Prefer DriverLib and SysConfig-generated macros over raw register writes.
- Keep app logic in `app/`, board abstractions in `bsp/`, reusable C++ helpers in
  `components/`, and chip/toolchain integration in `core/`.
- Do not edit vendored SDK, CMSIS, FreeRTOS, or DriverLib files unless fixing a
  project-blocking integration issue.
- Preserve interrupt handler names exactly as defined by the startup file and
  `ti_msp_dl_config.h`.
- For new IRQ users, confirm NVIC enable, interrupt priority, peripheral
  interrupt enable, and the exact handler symbol.
- Be conservative with RAM. This target has 32 KiB SRAM, and the current build
  already uses a meaningful portion of it.
- Avoid large stack buffers in tasks and ISRs.
- Check formatted output lengths before passing `snprintf`/`vsnprintf` results
  to UART or DMA send functions.
- Do not call blocking or task-only FreeRTOS APIs from interrupts. Use `FromISR`
  APIs where needed.

## CMake Conventions

- Top-level `CMakeLists.txt` owns the toolchain, target executable, link flags,
  post-build artifact generation, and flash target.
- `core/CMakeLists.txt` defines `ti_core` and imports DriverLib/CMSIS DSP
  archives.
- `bsp/CMakeLists.txt` builds the board support static library.
- `components/CMakeLists.txt` auto-loads component directories with their own
  `CMakeLists.txt` and links aliases named `components::<name>`.
- `app/CMakeLists.txt` recursively includes app subdirectories and builds app
  sources as an object library.

If adding a new component under `components/<name>`, provide a local
`CMakeLists.txt` and define a `components::<name>` alias so the aggregator links
it automatically.

## Submodules

Clone with submodules:

```powershell
git clone --recursive <repo-url>
```

If `components/utils` is missing:

```powershell
git submodule update --init --recursive
```

The root `.gitignore` intentionally ignores most `components/*` content while
keeping `components/CMakeLists.txt`, so remember that component code may live in
submodules.

## Validation Checklist

Before handing off a firmware change:

```powershell
cmake -S . -B cmake-build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug
```

If the task affects flashing or debug configuration:

```powershell
openocd -f daplink.cfg -c "shutdown"
```

If hardware is not connected, report validation as build/config-only. Do not
claim flashing or board behavior was verified without a connected board and
probe.
