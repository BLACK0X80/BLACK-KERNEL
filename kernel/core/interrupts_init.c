#include "../../include/drivers/pic.h"
#include "../../include/drivers/pit.h"
#include "../../include/kernel/idt.h"
#include "../../include/kernel/interrupts.h"
#include "../../include/kernel/port.h"

extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();
extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();
static idt_entry_t g_idt[256];
static idt_ptr_t g_idt_ptr;
static void idt_set_gate(int n, void *handler, uint16_t sel, uint8_t flags) {
  uint64_t h = (uint64_t)handler;
  g_idt[n].offset_low = (uint16_t)(h & 0xFFFF);
  g_idt[n].selector = sel;
  g_idt[n].ist = 0;
  g_idt[n].type_attr = flags;
  g_idt[n].offset_mid = (uint16_t)((h >> 16) & 0xFFFF);
  g_idt[n].offset_high = (uint32_t)((h >> 32) & 0xFFFFFFFF);
  g_idt[n].zero = 0;
}
void interrupts_init(void) {
  for (int i = 0; i < 256; i++) {
    idt_set_gate(i, 0, 0x08, 0x8E);
  }
  void *isrs[32] = {isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
                    isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
                    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
                    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31};
  for (int i = 0; i < 32; i++) {
    idt_set_gate(i, isrs[i], 0x08, 0x8E);
  }
  void *irqs[16] = {irq0, irq1, irq2,  irq3,  irq4,  irq5,  irq6,  irq7,
                    irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15};
  for (int i = 0; i < 16; i++) {
    idt_set_gate(32 + i, irqs[i], 0x08, 0x8E);
  }
  g_idt_ptr.limit = sizeof(g_idt) - 1;
  g_idt_ptr.base = (uint64_t)&g_idt[0];
  idt_load((uint64_t)&g_idt_ptr);
  pic_init();
  pit_init(1000);
  extern void cpu_enable_interrupts(void);
  cpu_enable_interrupts();
}
