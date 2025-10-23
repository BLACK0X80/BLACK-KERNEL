#include "../../include/kernel/port.h"
static inline void io_wait_internal(void) { outb(0x80, 0); }
uint8_t inb(uint16_t port) {
  uint8_t ret;
  __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}
void outb(uint16_t port, uint8_t value) {
  __asm__ volatile("outb %0, %1" ::"a"(value), "Nd"(port));
}
uint16_t inw(uint16_t port) {
  uint16_t ret;
  __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}
void outw(uint16_t port, uint16_t value) {
  __asm__ volatile("outw %0, %1" ::"a"(value), "Nd"(port));
}
uint32_t inl(uint16_t port) {
  uint32_t ret;
  __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}
void outl(uint16_t port, uint32_t value) {
  __asm__ volatile("outl %0, %1" ::"a"(value), "Nd"(port));
}
void io_wait(void) { io_wait_internal(); }
