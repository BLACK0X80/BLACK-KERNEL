#pragma once
#include "../kernel/types.h"
void keyboard_init(void);
void keyboard_irq_handler(void);
char keyboard_get_char(void);
int keyboard_has_input(void);
