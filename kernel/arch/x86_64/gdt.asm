BITS 64
GLOBAL gdt64_descriptor
GLOBAL gdt_init
GLOBAL gdt_load
SECTION .data
gdt64:
    dq 0
    dq 0x00AF9A000000FFFF
    dq 0x00AF92000000FFFF
    dq 0x00AFFA000000FFFF
    dq 0x00AFF2000000FFFF
    dq 0
gdt64_descriptor:
    dw gdt64_end - gdt64 - 1
    dq gdt64
gdt64_end:
SECTION .text
gdt_init:
    ret
gdt_load:
    lgdt [gdt64_descriptor]
    ret

