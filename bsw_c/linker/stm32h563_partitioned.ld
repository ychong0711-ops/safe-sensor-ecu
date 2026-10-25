/*
 * Week 5-6 Deliverable: Partitioned Linker Script for STM32H563ZI (ARM Cortex-M33, PMSAv8)
 * Strict 32-byte alignment on every protection boundary so `MPU_RBAR` / `MPU_RLAR` isolate
 * C BSW RAM from the Rust SWC stack & `.pim_rust_swc` with ZERO unaligned bleed-over bytes.
 */
ENTRY(Reset_Handler)

MEMORY
{
    FLASH_C_BSW    (rx)  : ORIGIN = 0x08000000, LENGTH = 256K
    FLASH_RUST_SWC (rx)  : ORIGIN = 0x08040000, LENGTH = 128K
    SRAM_C_BSW     (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
    SRAM_PIM_SHARE (rw)  : ORIGIN = 0x20010000, LENGTH = 4K
    SRAM_RUST_SWC  (rw)  : ORIGIN = 0x20011000, LENGTH = 16K
}

_estack_c_bsw = ORIGIN(SRAM_C_BSW) + LENGTH(SRAM_C_BSW);
_estack_rust  = ORIGIN(SRAM_RUST_SWC) + LENGTH(SRAM_RUST_SWC);

SECTIONS
{
    .isr_vector : ALIGN(256)
    {
        KEEP(*(.isr_vector))
    } > FLASH_C_BSW

    .text.c_bsw : ALIGN(32)
    {
        __c_flash_start__ = .;
        *(.text.bsw*)
        *(.rodata.bsw*)
        . = ALIGN(32);
        __c_flash_end__ = .;
    } > FLASH_C_BSW

    .text.rust_swc : ALIGN(32)
    {
        __rust_flash_start__ = .;
        *(.text*)
        *(.rodata*)
        . = ALIGN(32);
        __rust_flash_end__ = .;
    } > FLASH_RUST_SWC

    .ram_c_bsw : ALIGN(32)
    {
        __c_ram_start__ = .;
        *(.data.c_bsw*)
        *(.bss.c_bsw*)
        *(.data*)
        *(.bss*)
        *(COMMON)
        . = ALIGN(32);
        __c_ram_end__ = .;
    } > SRAM_C_BSW AT > FLASH_C_BSW

    .pim_rust_swc : ALIGN(32)
    {
        __pim_start__ = .;
        KEEP(*(.pim_rust_swc*))
        . = ALIGN(32);
        __pim_end__ = .;
    } > SRAM_PIM_SHARE AT > FLASH_C_BSW

    .ram_rust_swc (NOLOAD) : ALIGN(32)
    {
        __rust_ram_start__ = .;
        . = . + 4096; /* Dedicated 4 KB sandboxed stack for unprivileged Rust SWC */
        . = ALIGN(32);
        __rust_ram_end__ = .;
    } > SRAM_RUST_SWC
}
