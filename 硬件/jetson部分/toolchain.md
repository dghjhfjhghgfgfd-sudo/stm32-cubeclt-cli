# Toolchain Notes

## Detection

Prefer read-only checks before installing anything.

```bash
arm-none-eabi-gcc --version
arm-none-eabi-objcopy --version
make --version
STM32_Programmer_CLI --version
STM32_Programmer_CLI -l
openocd --version
```

On Windows, also check common install paths:

```powershell
Get-Command arm-none-eabi-gcc -ErrorAction SilentlyContinue
Get-Command STM32_Programmer_CLI -ErrorAction SilentlyContinue
Get-ChildItem "C:\Program Files\STMicroelectronics" -Recurse -Filter STM32_Programmer_CLI.exe -ErrorAction SilentlyContinue
```

On Linux/Jetson:

```bash
which arm-none-eabi-gcc make STM32_Programmer_CLI openocd || true
lsusb
ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || true
groups
```

## Preferred Tools

- STM32CubeCLT: vendor-supported CLI package with compiler/debug/programmer components.
- STM32CubeProgrammer CLI: best first choice for ST-Link and UART bootloader flashing.
- arm-none-eabi-gcc: compiler for Makefile/CMake projects.
- OpenOCD: useful ST-Link fallback, target config depends on family.
- stm32flash: simple UART bootloader fallback when CubeProgrammer is unavailable.

## Install Policy

Ask before installing or changing system packages. Avoid `apt upgrade` or broad package changes on fragile boards such as Jetson TX2.

For Jetson/Ubuntu, prefer a local toolchain tarball or already installed tools. If using apt, install only targeted packages after approval:

```bash
sudo apt-get update
sudo apt-get install -y gcc-arm-none-eabi make openocd
```

Do not assume CubeCLT exists on ARM Linux. If ST's package does not support the host architecture, use `arm-none-eabi-gcc` plus OpenOCD/stm32flash, or build on a separate PC and flash from the PC.

## Build Strategy

Use existing project files first:

- `Makefile`
- `CMakeLists.txt`
- `.ioc` plus generated code
- CubeCLT project files

If there is no project, generate a minimal no-HAL blink with `scripts/create_blink_project.py` to verify the toolchain. Only add HAL/CMSIS packages after the blink path works.

## Common Build Failures

- `arm-none-eabi-gcc: command not found`: compiler absent or PATH missing.
- `undefined reference to Reset_Handler`: startup file missing or vector table wrong.
- `region FLASH overflowed`: wrong linker script or target memory size.
- HAL header missing: CubeMX/Cube package not generated or include paths wrong.
- hard-float mismatch: inconsistent `-mfloat-abi` across objects/libraries.
- no blink after flash: wrong LED pin, wrong clock enable register, reset held low, BOOT0 high, or chip locked.
