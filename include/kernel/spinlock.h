#pragma once
#include "types.h"

typedef struct {
    volatile uint32_t locked;
} spinlock_t;

void spinlock_init(spinlock_t *lock);
void spinlock_acquire(spinlock_t *lock);
void spinlock_release(spinlock_t *lock);
int spinlock_try_acquire(spinlock_t *lock);
