#include "../../include/drivers/pic.h"
#include "../../include/kernel/port.h"
#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_COMMAND PIC1
#define PIC1_DATA (PIC1 + 1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA (PIC2 + 1)
#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01
void pic_init(void) {
  uint8_t a1 = inb(PIC1_DATA);
  uint8_t a2 = inb(PIC2_DATA);
  outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
  io_wait();
  outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
  io_wait();
  outb(PIC1_DATA, 0x20);
  io_wait();
  outb(PIC2_DATA, 0x28);
  io_wait();
  outb(PIC1_DATA, 4);
  io_wait();
  outb(PIC2_DATA, 2);
  io_wait();
  outb(PIC1_DATA, ICW4_8086);
  io_wait();
  outb(PIC2_DATA, ICW4_8086);
  io_wait();
  outb(PIC1_DATA, a1);
  outb(PIC2_DATA, a2);
}
void pic_send_eoi(uint8_t irq) {
  if (irq >= 8)
    outb(PIC2_COMMAND, 0x20);
  outb(PIC1_COMMAND, 0x20);
}
void pic_mask_irq(uint8_t irq) {
  uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
  uint8_t val = inb(port);
  if (irq < 8)
    outb(port, val | (1 << (irq)));
  else
    outb(port, val | (1 << (irq - 8)));
}
void pic_unmask_irq(uint8_t irq) {
  uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
  uint8_t val = inb(port);
  if (irq < 8)
    outb(port, val & ~(1 << (irq)));
  else
    outb(port, val & ~(1 << (irq - 8)));
}
