BITS 32
SECTION .multiboot
align 8
MB2_MAGIC equ 0xE85250D6
MB2_ARCH  equ 0
MB2_LENGTH equ multiboot_end - multiboot_start
MB2_CHECKSUM equ -(MB2_MAGIC + MB2_ARCH + MB2_LENGTH)
multiboot_start:
    dd MB2_MAGIC
    dd MB2_ARCH
    dd MB2_LENGTH
    dd MB2_CHECKSUM
    dw 0
    dw 0
    dd 8
multiboot_end:

