#!/usr/bin/env bash
set -euo pipefail
make -j$(nproc)
qemu-system-x86_64 -kernel build/prometheus-kernel.elf -serial stdio -s -S

