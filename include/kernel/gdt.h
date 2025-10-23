#pragma once
#include "types.h"
void gdt_load(uint64_t gdt_ptr);
void gdt_init(void);
