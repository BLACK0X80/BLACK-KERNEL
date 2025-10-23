#pragma once
#include "types.h"
void kernel_panic(const char *message, const char *file, uint32_t line);
#define PANIC(msg) kernel_panic((msg), __FILE__, __LINE__)
