OUTPUT_FORMAT("elf32-avr", "elf32-avr", "elf32-avr")
OUTPUT_ARCH(avr:5)

MEMORY
{
    FLASH       (rx)    : ORIGIN = 0x000000, LENGTH = 32K
    SRAM        (rw!x)    : ORIGIN = 0x800100, LENGTH = 2K
    EEPROM      (rw!x)  : ORIGIN = 0x810000, LENGTH = 1K
}

SECTIONS
{

    .text :
    {
        *(.vectors)       /* interrupt vector table MUST be first — CPU jumps here on reset */
        KEEP(*(.vectors)) /* don't discard even if "unused" */
        *(.init*)         /* startup code */
        *(.text*)         /* all compiled C and assembly code */
        *(.rodata*)       /* string literals and const globals */
        . = ALIGN(2);     /* pad to even address — AVR instructions are 2 bytes wide */
    }> FLASH

    .data : AT (ADDR(.text) + SIZEOF(.text))
    {
        __data_start_sram = .;  /* export symbol */
        *(.data*)
        *(.rodata*)             /* string literals and const globals moved to SRAM */
        . = ALIGN(2);

        __data_end_sram = .;  /* export symbol */
    }>SRAM

    /* export the Flash address so startup.S knows where to copy from */
    __data_start_flash = LOADADDR(.data);
    __data_end_flash    = __data_start_flash + SIZEOF(.data);

    .bss :
    {
        __bss_start_sram = .;
        *(.bss*)
        *(COMMON)         /* uninitialized globals that C compilers put in COMMON */
        __bss_end_sram = .;
    } > SRAM

    /* Size helpers for startup.S — placed after all sections are defined */
    __data_bytes_to_read = SIZEOF(.data);
    __bss_bytes_to_clear = SIZEOF(.bss);
}
