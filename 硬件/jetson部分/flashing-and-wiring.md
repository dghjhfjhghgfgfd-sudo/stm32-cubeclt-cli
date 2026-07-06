# Flashing And Wiring

## Preferred ST-Link Wiring

Use SWD for first bring-up when possible:

```text
ST-Link SWDIO -> STM32 SWDIO
ST-Link SWCLK -> STM32 SWCLK
ST-Link GND   -> STM32 GND
ST-Link 3V3   -> target VREF only, not necessarily power
NRST optional -> STM32 NRST
```

Power the target from a known stable supply. Common small boards can be powered from ST-Link 3V3, but motors, relays, servos, and external modules must use separate power.

Check connection:

```bash
STM32_Programmer_CLI -l
STM32_Programmer_CLI -c port=SWD
```

Flash:

```bash
STM32_Programmer_CLI -c port=SWD -w build/firmware.hex -v -rst
```

## UART Bootloader Wiring

Use UART bootloader only when ST-Link is unavailable.

Typical STM32 USART1 bootloader pins:

```text
USB-TTL TX -> STM32 RX, often PA10
USB-TTL RX -> STM32 TX, often PA9
USB-TTL GND -> STM32 GND
```

Enter bootloader:

```text
BOOT0 = 1
BOOT1 = 0 if present
Reset pulse
Flash
BOOT0 = 0
Reset again
```

CubeProgrammer example:

```bash
STM32_Programmer_CLI -c port=/dev/ttyUSB0 br=115200 -w build/firmware.hex -v -rst
```

stm32flash example:

```bash
stm32flash -b 115200 -w build/firmware.bin -v -g 0x0 /dev/ttyUSB0
```

## Jetson/Linux Serial Notes

Check serial devices:

```bash
lsusb
ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || true
groups
```

If permission fails:

```bash
sudo usermod -aG dialout $USER
# log out and back in
```

Jetson TX2 caveat: old JetPack kernels may lack CH340 (`ch341`) support. If `lsusb` sees `1a86:7523` but no `/dev/ttyUSB0`, the kernel needs a matching `ch341.ko` module or a CP2102/FTDI/PL2303 adapter.

## Safety Rules

- Always common GND between controller and target when using UART/PWM/GPIO.
- Never feed 5V signals into 3.3V-only STM32 pins unless the pin is 5V tolerant and the mode allows it.
- Do not power motors or high-torque servos from a debugger, Jetson header, or MCU board regulator.
- Add a stop command and reset path before motor tests.
- Start with blink, then UART echo, then one motor/servo at low speed.

## Common Flash Failures

- `No ST-LINK detected`: cable, driver, permission, or ST-Link firmware issue.
- `Cannot connect`: target unpowered, SWD swapped, reset held, BOOT0 wrong, low-power firmware locks SWD.
- UART timeout: TX/RX swapped, BOOT0 not high, wrong UART pins, wrong baud, no common ground.
- Verify failure: wrong flash address, wrong chip family, readout protection, unstable power.
