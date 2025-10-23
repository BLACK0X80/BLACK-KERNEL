#include "../../include/drivers/serial.h"
#include "../../include/kernel/port.h"
#define COM1_PORT 0x3F8
void serial_init(void) {
  outb(COM1_PORT + 1, 0x00);
  outb(COM1_PORT + 3, 0x80);
  outb(COM1_PORT + 0, 0x03);
  outb(COM1_PORT + 1, 0x00);
  outb(COM1_PORT + 3, 0x03);
  outb(COM1_PORT + 2, 0xC7);
  outb(COM1_PORT + 4, 0x0B);
}
int serial_received(void) { return inb(COM1_PORT + 5) & 1; }
char serial_read_char(void) {
  while (!serial_received())
    ;
  return inb(COM1_PORT);
}
static int serial_is_transmit_empty(void) { return inb(COM1_PORT + 5) & 0x20; }
void serial_write_char(char c) {
  while (!serial_is_transmit_empty())
    ;
  outb(COM1_PORT, (uint8_t)c);
}
void serial_write_string(const char *str) {
  if (!str)
    return;
  while (*str) {
    serial_write_char(*str++);
  }
}
