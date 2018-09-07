ENTRY(_start)     /* Address for bootloader to jump to. */
SECTIONS
{
        . = 1M;   /* Load the sections at address 1MB (0x100000) */
        .text BLOCK(4K) : ALIGN(4K)
        {
                *(.multiboot)
                *(.text)
        }
        .rodata BLOCK(4K) : ALIGN(4K)
        {
                *(.rodata)
        }
        .data BLOCK(4K) : ALIGN(4K)
        {
                *(.data)
        }
        .bss BLOCK(4K) : ALIGN(4K)
        {
                *(COMMON)
                *(.bss)
        }
}
