#include "../../include/drivers/pit.h"
#include "../../include/kernel/port.h"
static volatile uint64_t g_pit_ticks = 0;
void pit_init(uint32_t frequency) {
  if (frequency == 0)
    frequency = 1000;
  uint32_t divisor = 1193182 / frequency;
  outb(0x43, 0x36);
  outb(0x40, (uint8_t)(divisor & 0xFF));
  outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}
uint64_t pit_get_ticks(void) { return g_pit_ticks; }
void pit_wait(uint32_t milliseconds) {
  uint64_t target = g_pit_ticks + (uint64_t)milliseconds;
  while (g_pit_ticks < target) {
    __asm__ volatile("hlt");
  }
}
void pit_irq_tick(void) { g_pit_ticks++; }
