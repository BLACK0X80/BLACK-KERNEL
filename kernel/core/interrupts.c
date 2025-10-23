#include "../../include/kernel/interrupts.h"
#include "../../include/drivers/keyboard.h"
#include "../../include/drivers/pic.h"
#include "../../include/drivers/pit.h"
#include "../../include/kernel/stdio.h"

void interrupt_handler(interrupt_frame_t *frame) {
  if (frame->int_no < 32) {
    kprintf("Exception %u err=%u\n", (unsigned)frame->int_no,
            (unsigned)frame->err_code);
    while (1) {
      __asm__ volatile("hlt");
    }
  }
  if (frame->int_no >= 32 && frame->int_no <= 47) {
    uint8_t irq = (uint8_t)(frame->int_no - 32);
    if (irq == 0) {
      extern void pit_irq_tick(void);
      pit_irq_tick();
    }
    if (irq == 1) {
      keyboard_irq_handler();
    }
    pic_send_eoi(irq);
  }
}
