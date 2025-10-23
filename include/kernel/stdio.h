#pragma once
#include "types.h"
#include <stdarg.h>

int kprintf(const char *format, ...);
int ksprintf(char *buffer, const char *format, ...);
void kputs(const char *str);
