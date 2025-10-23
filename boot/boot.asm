BITS 32
GLOBAL _start
EXTERN gdt64_descriptor
EXTERN kernel_main
SECTION .text
VGA_MEM equ 0xB8000
STACK_SIZE equ 16384
CR0_PG equ 0x80000000
CR0_PE equ 0x1
CR4_PAE equ 0x20
EFER_MSR equ 0xC0000080
EFER_LME equ 0x00000100
PRESENT_RW_PS equ 0x83
ALIGN_PAGE equ 4096
SECTION .bss
align 16
stack_bottom: resb STACK_SIZE
stack_top:
SECTION .bss
align 4096
pml4:   resq 512
pdpt:   resq 512
pd:     resq 512
SECTION .bss
align 8
mb_magic_store: resq 1
mb_info_store:  resq 1
SECTION .text
_start:
    cmp eax, 0x36D76289
    jne boot_error
    mov esp, stack_top
    mov [mb_magic_store], eax
    mov [mb_info_store], ebx
    call check_cpuid
    call check_long_mode
    call setup_paging
    lgdt [gdt64_descriptor]
    mov eax, cr4
    or eax, CR4_PAE
    mov cr4, eax
    mov ecx, EFER_MSR
    rdmsr
    or eax, EFER_LME
    wrmsr
    mov eax, pml4
    mov cr3, eax
    mov eax, cr0
    or eax, CR0_PG
    mov cr0, eax
    jmp 0x08:long_mode_label
boot_error:
    mov edi, VGA_MEM
    mov eax, 0x4F204F45
    mov [edi], eax
    hlt
check_cpuid:
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1<<21
    push eax
    popfd
    pushfd
    pop eax
    xor eax, ecx
    test eax, 1<<21
    jz boot_error
    ret
check_long_mode:
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb boot_error
    mov eax, 0x80000001
    cpuid
    test edx, 1<<29
    jz boot_error
    ret
setup_paging:
    mov edi, pml4
    mov ecx, 512*8
    xor eax, eax
    rep stosb
    mov edi, pdpt
    mov ecx, 512*8
    xor eax, eax
    rep stosb
    mov edi, pd
    mov ecx, 512*8
    xor eax, eax
    rep stosb
    mov rax, pdpt
    or rax, 0x3
    mov [pml4], rax
    mov rax, pd
    or rax, 0x3
    mov [pdpt], rax
    mov rcx, 512
    xor rbx, rbx
.map_loop:
    mov rax, rbx
    shl rax, 21
    or rax, PRESENT_RW_PS
    mov [pd + rbx*8], rax
    inc rbx
    loop .map_loop
    mov rax, pdpt
    or rax, 0x3
    mov [pml4 + 510*8], rax
    ret
BITS 64
long_mode_label:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    lea rsp, [rel stack_top]
    mov rdi, [rel mb_magic_store]
    mov rsi, [rel mb_info_store]
    call kernel_main
    hlt

