#pragma once
#include "../kernel/types.h"
#include <stdarg.h>

void serial_init(void);
void serial_write_char(char c);
void serial_write_string(const char *str);
int serial_received(void);
char serial_read_char(void);
