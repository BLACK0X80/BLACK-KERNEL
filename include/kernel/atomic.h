#pragma once
#include "types.h"

static inline uint32_t atomic_compare_and_swap(volatile uint32_t *ptr, uint32_t expected, uint32_t desired) {
    uint32_t prev;
    __asm__ volatile(
        "lock cmpxchgl %2, %1"
        : "=a"(prev), "+m"(*ptr)
        : "r"(desired), "0"(expected)
        : "memory"
    );
    return prev;
}

static inline uint32_t atomic_fetch_and_add(volatile uint32_t *ptr, uint32_t value) {
    uint32_t prev;
    __asm__ volatile(
        "lock xaddl %0, %1"
        : "=r"(prev), "+m"(*ptr)
        : "0"(value)
        : "memory"
    );
    return prev;
}

static inline void atomic_store(volatile uint32_t *ptr, uint32_t value) {
    __asm__ volatile(
        "movl %1, %0"
        : "=m"(*ptr)
        : "r"(value)
        : "memory"
    );
}

static inline uint32_t atomic_load(volatile uint32_t *ptr) {
    uint32_t value;
    __asm__ volatile(
        "movl %1, %0"
        : "=r"(value)
        : "m"(*ptr)
        : "memory"
    );
    return value;
}

static inline void memory_barrier(void) {
    __asm__ volatile("mfence" ::: "memory");
}
