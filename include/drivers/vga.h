#pragma once
#include "../kernel/types.h"
void vga_init(void);
void vga_clear(void);
void vga_putchar(char c);
void vga_write(const char *str);
void vga_set_color(uint8_t foreground, uint8_t background);
void vga_set_cursor(uint8_t x, uint8_t y);
