#pragma once
#include "types.h"
#define MULTIBOOT2_MAGIC 0x36d76289
typedef struct __attribute__((packed)) {
  uint32_t type;
  uint32_t size;
} multiboot_tag_t;
typedef struct __attribute__((packed)) {
  uint32_t type;
  uint32_t size;
  uint32_t entry_size;
  uint32_t entry_version;
} multiboot_tag_mmap_t;
typedef struct __attribute__((packed)) {
  uint64_t addr;
  uint64_t len;
  uint32_t type;
  uint32_t zero;
} multiboot_mmap_entry_t;
