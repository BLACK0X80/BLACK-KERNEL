Prometheus Kernel

Overview
Prometheus is a freestanding x86_64 kernel booted via Multiboot2. It brings up 64-bit long mode, initializes GDT/IDT, PIC/PIT, basic drivers, and provides PMM/VMM and a simple heap.

Features

- Multiboot2 32->64 bit transition
- Long mode with 2MiB early paging then 4-level paging API
- GDT/IDT with ISR/IRQ stubs and common handler
- PIC remap, PIT timer (1000Hz), PS/2 keyboard
- VGA text mode and COM1 serial I/O
- PMM bitmap allocator, VMM page mapping API
- Simple first-fit heap allocator

Build
Requirements: nasm, x86_64-elf-gcc, x86_64-elf-ld, grub-mkrescue, qemu

Windows: use WSL or a cross toolchain providing the above.

Commands:
make
./run.sh
./debug.sh

Run
make run will launch QEMU and print initialization logs to VGA and serial.

Debug
./debug.sh starts QEMU with a GDB server on tcp::1234. Connect using gdb target remote :1234.

Architecture
boot/ contains the Multiboot2 header and paging bring-up.
kernel/arch/x86_64/ contains low-level assembly for entry, GDT, IDT, and interrupt stubs.
kernel/core/ contains initialization, panic, and interrupt management.
kernel/mm/ contains PMM (bitmap), VMM (page tables), and heap.
kernel/drivers/ contains VGA, serial, PS/2 keyboard, PIT, and PIC drivers.
include/ exposes public headers.

Memory Map

- Kernel loaded at 1MiB physical, mapped to higher half 0xFFFFFFFF80000000.
- Heap starts at 0x0000000080000000 with 16MiB region.

Contributing
Use consistent naming and no comments in code. Submit PRs with clear commit messages and tested changes.

License
MIT
