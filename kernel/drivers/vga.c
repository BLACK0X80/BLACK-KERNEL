#include "../../include/drivers/vga.h"
#include "../../include/kernel/port.h"
#include "../../include/kernel/types.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000
#define CURSOR_CMD 0x3D4
#define CURSOR_DATA 0x3D5
static uint16_t *g_vga_buffer = (uint16_t *)(uintptr_t)VGA_MEMORY;
static uint8_t g_vga_color = 0x07;
static uint8_t g_cursor_x = 0;
static uint8_t g_cursor_y = 0;
static uint16_t vga_entry(char c, uint8_t color) {
  return (uint16_t)c | ((uint16_t)color << 8);
}
static void vga_move_cursor(void) {
  uint16_t pos = g_cursor_y * VGA_WIDTH + g_cursor_x;
  outb(CURSOR_CMD, 0x0F);
  outb(CURSOR_DATA, (uint8_t)(pos & 0xFF));
  outb(CURSOR_CMD, 0x0E);
  outb(CURSOR_DATA, (uint8_t)((pos >> 8) & 0xFF));
}
void vga_init(void) {
  g_cursor_x = 0;
  g_cursor_y = 0;
  g_vga_color = 0x07;
  vga_clear();
}
void vga_clear(void) {
  for (int y = 0; y < VGA_HEIGHT; y++) {
    for (int x = 0; x < VGA_WIDTH; x++) {
      g_vga_buffer[y * VGA_WIDTH + x] = vga_entry(' ', g_vga_color);
    }
  }
  g_cursor_x = 0;
  g_cursor_y = 0;
  vga_move_cursor();
}
void vga_set_color(uint8_t foreground, uint8_t background) {
  g_vga_color = (background << 4) | (foreground & 0x0F);
}
static void vga_scroll(void) {
  if (g_cursor_y < VGA_HEIGHT)
    return;
  for (int y = 1; y < VGA_HEIGHT; y++) {
    for (int x = 0; x < VGA_WIDTH; x++) {
      g_vga_buffer[(y - 1) * VGA_WIDTH + x] = g_vga_buffer[y * VGA_WIDTH + x];
    }
  }
  for (int x = 0; x < VGA_WIDTH; x++) {
    g_vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] =
        vga_entry(' ', g_vga_color);
  }
  g_cursor_y = VGA_HEIGHT - 1;
}
void vga_putchar(char c) {
  if (c == '\n') {
    g_cursor_x = 0;
    g_cursor_y++;
  } else if (c == '\r') {
    g_cursor_x = 0;
  } else if (c == '\t') {
    g_cursor_x = (g_cursor_x + 8) & ~7;
  } else {
    g_vga_buffer[g_cursor_y * VGA_WIDTH + g_cursor_x] =
        vga_entry(c, g_vga_color);
    g_cursor_x++;
    if (g_cursor_x >= VGA_WIDTH) {
      g_cursor_x = 0;
      g_cursor_y++;
    }
  }
  vga_scroll();
  vga_move_cursor();
}
void vga_write(const char *str) {
  if (!str)
    return;
  while (*str) {
    vga_putchar(*str++);
  }
}
void vga_set_cursor(uint8_t x, uint8_t y) {
  if (x >= VGA_WIDTH)
    x = VGA_WIDTH - 1;
  if (y >= VGA_HEIGHT)
    y = VGA_HEIGHT - 1;
  g_cursor_x = x;
  g_cursor_y = y;
  vga_move_cursor();
}
