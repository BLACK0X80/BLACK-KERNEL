#include "../../include/kernel/panic.h"
#include "../../include/drivers/serial.h"
#include "../../include/drivers/vga.h"
#include "../../include/kernel/interrupts.h"

extern void cpu_disable_interrupts(void);
void kernel_panic(const char *message, const char *file, uint32_t line) {
  cpu_disable_interrupts();
  vga_set_color(0x0F, 0x04);
  vga_clear();
  serial_write_string("PANIC ");
  serial_write_string(message ? message : "(null)");
  serial_write_string("\n");
  while (1) {
    __asm__ volatile("hlt");
  }
}
