#include "../../include/kernel/spinlock.h"
#include "../../include/kernel/atomic.h"

void spinlock_init(spinlock_t *lock) {
    atomic_store(&lock->locked, 0);
}

void spinlock_acquire(spinlock_t *lock) {
    while (atomic_compare_and_swap(&lock->locked, 0, 1) != 0) {
        while (atomic_load(&lock->locked) != 0) {
            __asm__ volatile("pause");
        }
    }
    memory_barrier();
}

void spinlock_release(spinlock_t *lock) {
    memory_barrier();
    atomic_store(&lock->locked, 0);
}

int spinlock_try_acquire(spinlock_t *lock) {
    if (atomic_compare_and_swap(&lock->locked, 0, 1) == 0) {
        memory_barrier();
        return 1;
    }
    return 0;
}
