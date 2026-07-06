---
name: stm32-cubeclt-cli
description: Command-line STM32 development workflow for Codex/agents. Use when creating, configuring, building, flashing, or debugging STM32 projects without Keil or CubeIDE GUI, especially with STM32CubeCLT, arm-none-eabi-gcc, STM32_Programmer_CLI, OpenOCD, ST-Link, UART bootloader, Jetson/Linux hosts, TTL serial, blink bring-up, HAL/LL drivers, Makefiles, linker scripts, or embedded contest rapid prototyping.
---

# STM32 CubeCLT CLI

## Core Rule

Prefer the smallest reversible step that proves hardware is alive:

1. Detect tools and target.
2. Build a minimal blink or UART echo.
3. Flash with ST-Link/SWD when available.
4. Use UART bootloader only when ST-Link is unavailable.
5. Add peripherals one at a time.

Do not run broad OS upgrades or replace vendor packages unless the user explicitly approves the risk.

## Workflow

### 1. Identify The Board

Ask for or infer:

- MCU part number, such as `STM32F103C8T6`, `STM32F407VET6`, `STM32G431CBU6`, or `STM32H743`.
- Board type and clock source.
- LED pin, reset/boot pins, UART pins, and debugger type.
- Host OS: Windows, Linux, Jetson, WSL, or macOS.

If the exact MCU is unknown, stop at environment detection and hardware inventory.

### 2. Detect Environment

Run non-destructive checks first:

```bash
arm-none-eabi-gcc --version
make --version
STM32_Programmer_CLI --version
STM32_Programmer_CLI -l
openocd --version
```

On Linux/Jetson, also check:

```bash
lsusb
ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || true
groups
```

If ST-Link appears but access fails, add the user to `dialout` or install udev rules only with user approval.

### 3. Choose Build Path

Use this order:

1. Existing CubeMX/CubeCLT project if present.
2. Existing Makefile/CMake project if present.
3. Minimal generated blink project from `scripts/create_blink_project.py`.
4. User-approved CubeCLT/CubeMX generation or template import.

For generated minimal projects, keep HAL optional. A register-level blink is acceptable for first power-on proof when HAL packages are missing.

### 4. Build

Use the repo's existing build command if available. Otherwise create a small Makefile with:

- `arm-none-eabi-gcc`
- correct `-mcpu`, `-mthumb`, and float ABI
- startup file
- linker script
- `build/*.elf`, `build/*.hex`, `build/*.bin`

If the error is missing CMSIS/HAL files, do not invent them. Report the missing package and offer a minimal no-HAL blink or CubeCLT install path.

### 5. Flash

Prefer ST-Link:

```bash
STM32_Programmer_CLI -l
STM32_Programmer_CLI -c port=SWD -w build/firmware.hex -v -rst
```

OpenOCD fallback:

```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program build/firmware.elf verify reset exit"
```

UART bootloader fallback:

1. BOOT0 high, BOOT1 low if present.
2. Reset target.
3. Use `STM32_Programmer_CLI -c port=/dev/ttyUSB0 br=115200` or `stm32flash`.
4. Restore BOOT0 low and reset after flashing.

Read `references/flashing-and-wiring.md` before TTL bootloader or Jetson serial work.

### 6. Validate

Always report:

- Tool versions found.
- MCU/board assumptions.
- Build output path and size.
- Flash method and exact command.
- Observed behavior: LED blink, UART print, or error.
- Next safest step.

## Bundled Resources

- `scripts/create_blink_project.py`: generate a minimal Makefile-based blink skeleton.
- `references/toolchain.md`: install/detection notes for CubeCLT, GCC, OpenOCD, and STM32_Programmer_CLI.
- `references/flashing-and-wiring.md`: ST-Link, UART bootloader, Jetson TTL, and common hardware faults.
