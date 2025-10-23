#pragma once
#include "types.h"
typedef struct __attribute__((packed)) {
  uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
  uint64_t rsi, rdi, rbp, rdx, rcx, rbx, rax;
  uint64_t int_no, err_code;
  uint64_t rip, cs, rflags, rsp, ss;
} interrupt_frame_t;
void interrupt_handler(interrupt_frame_t *frame);
void interrupts_init(void);
