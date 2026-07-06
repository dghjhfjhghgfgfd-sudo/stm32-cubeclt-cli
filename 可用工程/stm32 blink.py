#!/usr/bin/env python3
"""Generate a minimal register-level STM32 blink project.

This scaffold is intended for bring-up: prove compiler, linker, startup,
and flashing path before adding HAL/CubeMX-generated code.
"""
from __future__ import annotations

import argparse
from pathlib import Path

TARGETS = {
    "stm32f103c8": {
        "cpu": "cortex-m3",
        "flash_origin": "0x08000000",
        "flash_len": "64K",
        "ram_origin": "0x20000000",
        "ram_len": "20K",
        "rcc_apb2enr": "0x40021018",
        "gpio_base": "0x40011000",  # GPIOC
        "cr_offset": "0x04",        # CRH
        "odr_offset": "0x0C",
        "pin": 13,
        "pin_cfg_comment": "PC13 output push-pull, 2 MHz",
        "pin_cfg_code": "*gpio_cr = (*gpio_cr & ~(0xFu << 20)) | (0x2u << 20);",
        "openocd_target": "stm32f1x.cfg",
    },
    "stm32f407vet": {
        "cpu": "cortex-m4",
        "flash_origin": "0x08000000",
        "flash_len": "512K",
        "ram_origin": "0x20000000",
        "ram_len": "128K",
        "rcc_apb2enr": None,
        "rcc_ahb1enr": "0x40023830",
        "gpio_base": "0x40020C00",  # GPIOD
        "moder_offset": "0x00",
        "odr_offset": "0x14",
        "pin": 12,
        "pin_cfg_comment": "PD12 output",
        "pin_cfg_code": "*gpio_moder = (*gpio_moder & ~(0x3u << 24)) | (0x1u << 24);",
        "openocd_target": "stm32f4x.cfg",
    },
}


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def linker_script(t: dict) -> str:
    return f"""ENTRY(Reset_Handler)

MEMORY
{{
  FLASH (rx) : ORIGIN = {t['flash_origin']}, LENGTH = {t['flash_len']}
  RAM (rwx)  : ORIGIN = {t['ram_origin']}, LENGTH = {t['ram_len']}
}}

_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{{
  .isr_vector : {{ KEEP(*(.isr_vector)) }} > FLASH
  .text : {{ *(.text*) *(.rodata*) KEEP(*(.init)) KEEP(*(.fini)) }} > FLASH
  .ARM.exidx : {{ *(.ARM.exidx*) }} > FLASH
  _sidata = LOADADDR(.data);
  .data : AT(_sidata) {{ _sdata = .; *(.data*) _edata = .; }} > RAM
  .bss : {{ _sbss = .; *(.bss*) *(COMMON) _ebss = .; }} > RAM
}}
"""


def startup_c() -> str:
    return r'''#include <stdint.h>

extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;
int main(void);

void Default_Handler(void) { while (1) {} }
void Reset_Handler(void) {
    uint32_t *src = &_sidata;
    for (uint32_t *dst = &_sdata; dst < &_edata;) { *dst++ = *src++; }
    for (uint32_t *dst = &_sbss; dst < &_ebss;) { *dst++ = 0; }
    main();
    while (1) {}
}

__attribute__((section(".isr_vector")))
void (* const vector_table[])(void) = {
    (void (*)(void))(&_estack),
    Reset_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler,
};
'''


def main_c(target: str, t: dict) -> str:
    pin_mask = f"(1u << {t['pin']})"
    if target.startswith("stm32f103"):
        clock = f"#define RCC_APB2ENR (*(volatile uint32_t *){t['rcc_apb2enr']})\n#define GPIO_CLOCK_BIT (1u << 4)\n"
        enable = "RCC_APB2ENR |= GPIO_CLOCK_BIT;"
        regs = f"volatile uint32_t *gpio_cr = (uint32_t *)(GPIO_BASE + {t['cr_offset']});\n"
    else:
        clock = f"#define RCC_AHB1ENR (*(volatile uint32_t *){t['rcc_ahb1enr']})\n#define GPIO_CLOCK_BIT (1u << 3)\n"
        enable = "RCC_AHB1ENR |= GPIO_CLOCK_BIT;"
        regs = f"volatile uint32_t *gpio_moder = (uint32_t *)(GPIO_BASE + {t['moder_offset']});\n"
    return f'''#include <stdint.h>

{clock}#define GPIO_BASE ((uintptr_t){t['gpio_base']})
#define LED_MASK {pin_mask}

static void delay(volatile uint32_t n) {{ while (n--) {{ __asm volatile("nop"); }} }}

int main(void) {{
    {enable}
    {regs}    volatile uint32_t *gpio_odr = (uint32_t *)(GPIO_BASE + {t['odr_offset']});

    // {t['pin_cfg_comment']}.
    {t['pin_cfg_code']}

    while (1) {{
        *gpio_odr ^= LED_MASK;
        delay(800000);
    }}
}}
'''


def makefile(target: str, t: dict) -> str:
    return f'''TARGET = firmware
MCU = -mcpu={t['cpu']} -mthumb
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size
CFLAGS = $(MCU) -Wall -Wextra -Os -ffunction-sections -fdata-sections -nostdlib
LDFLAGS = $(MCU) -T linker.ld -Wl,--gc-sections -Wl,-Map=build/$(TARGET).map -nostdlib
SRCS = src/startup.c src/main.c
OBJS = $(SRCS:src/%.c=build/%.o)

all: build/$(TARGET).elf build/$(TARGET).hex build/$(TARGET).bin
	$(SIZE) build/$(TARGET).elf

build/%.o: src/%.c
	mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/$(TARGET).elf: $(OBJS) linker.ld
	$(CC) $(OBJS) $(LDFLAGS) -o $@

build/$(TARGET).hex: build/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

build/$(TARGET).bin: build/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

flash-stlink: build/$(TARGET).hex
	STM32_Programmer_CLI -c port=SWD -w build/$(TARGET).hex -v -rst

flash-openocd: build/$(TARGET).elf
	openocd -f interface/stlink.cfg -f target/{t['openocd_target']} -c "program build/$(TARGET).elf verify reset exit"

clean:
	rm -rf build
'''


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", required=True, choices=sorted(TARGETS))
    parser.add_argument("--out", required=True, help="Output project directory")
    args = parser.parse_args()

    out = Path(args.out).resolve()
    t = TARGETS[args.target]
    write(out / "src" / "startup.c", startup_c())
    write(out / "src" / "main.c", main_c(args.target, t))
    write(out / "linker.ld", linker_script(t))
    write(out / "Makefile", makefile(args.target, t))
    print(f"Created {args.target} blink project at {out}")
    print("Next: cd", out)
    print("Run: make")


if __name__ == "__main__":
    main()
