#include "../../include/drivers/keyboard.h"
#include "../../include/kernel/port.h"
static volatile char g_kbd_buf[256];
static volatile uint16_t g_kbd_head = 0, g_kbd_tail = 0;
static int g_shift = 0, g_caps = 0;
static const char map_norm[128] = {
    0,   27,   '1',  '2', '3',  '4', '5', '6', '7', '8', '9', '0', '-',
    '=', 8,    '\t', 'q', 'w',  'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
    '[', ']',  '\n', 0,   'a',  's', 'd', 'f', 'g', 'h', 'j', 'k', 'l',
    ';', '\'', '`',  0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',',
    '.', '/',  0,    '*', 0,    ' ', 0,   0,   0,   0,   0,   0,   0,
    0,   0,    0,    0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,    0,    0,   0,    0,   0,   0,   0,   0};
static const char map_shift[128] = {
    0,    27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',  8,
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A',  'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,   '|',  'Z',
    'X',  'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0};
static void kbd_push(char c) {
  uint16_t next = (g_kbd_head + 1) & 255;
  if (next != g_kbd_tail) {
    g_kbd_buf[g_kbd_head] = c;
    g_kbd_head = next;
  }
}
void keyboard_init(void) {}
void keyboard_irq_handler(void) {
  uint8_t sc = inb(0x60);
  if (sc == 0x2A || sc == 0x36) {
    g_shift = 1;
    return;
  }
  if (sc == 0xAA || sc == 0xB6) {
    g_shift = 0;
    return;
  }
  if (sc == 0x3A) {
    g_caps ^= 1;
    return;
  }
  if (sc & 0x80)
    return;
  char c = g_shift ? map_shift[sc] : map_norm[sc];
  if (c >= 'a' && c <= 'z') {
    if (g_caps ^ g_shift)
      c = c - 32;
  }
  if (c)
    kbd_push(c);
}
char keyboard_get_char(void) {
  if (g_kbd_head == g_kbd_tail)
    return 0;
  char c = g_kbd_buf[g_kbd_tail];
  g_kbd_tail = (g_kbd_tail + 1) & 255;
  return c;
}
int keyboard_has_input(void) { return g_kbd_head != g_kbd_tail; }
