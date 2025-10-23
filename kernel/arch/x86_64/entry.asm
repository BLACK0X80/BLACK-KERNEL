BITS 64
GLOBAL long_mode_entry
EXTERN kernel_main
SECTION .text
long_mode_entry:
    call kernel_main
.halt:
    hlt
    jmp .halt

