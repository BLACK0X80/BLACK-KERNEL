BITS 64
GLOBAL cpu_halt
GLOBAL cpu_disable_interrupts
GLOBAL cpu_enable_interrupts
SECTION .text
cpu_halt:
.l: hlt
    jmp .l
cpu_disable_interrupts:
    cli
    ret
cpu_enable_interrupts:
    sti
    ret

