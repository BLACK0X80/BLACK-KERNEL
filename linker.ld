ENTRY(_start)

SECTIONS
{
    . = 1M;

    .boot : {
        KEEP(*(.multiboot))
    }

    . += 0xFFFFFFFF80000000;

    .text ALIGN(4K) : AT(ADDR(.text) - 0xFFFFFFFF80000000) {
        *(.text .text.*)
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - 0xFFFFFFFF80000000) {
        *(.rodata .rodata.*)
    }

    .data ALIGN(4K) : AT(ADDR(.data) - 0xFFFFFFFF80000000) {
        *(.data .data.*)
    }

    .bss ALIGN(4K) : AT(ADDR(.bss) - 0xFFFFFFFF80000000) {
        *(COMMON)
        *(.bss .bss.*)
        *(COMMON)
    }

    kernel_end = .;
}

