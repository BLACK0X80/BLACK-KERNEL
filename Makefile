ARCH=x86_64
CC=x86_64-elf-gcc
LD=x86_64-elf-ld
AS=nasm
OBJCOPY=x86_64-elf-objcopy
CFLAGS=-ffreestanding -nostdlib -nostdinc -mno-red-zone -mcmodel=kernel -O2 -Wall -Wextra -std=gnu11 -fno-stack-protector -fno-pic
LDFLAGS=-T linker.ld -nostdlib
ASFLAGS=-f elf64
ASFLAGS32=-f elf32

BUILD_DIR=build
ISO_DIR=$(BUILD_DIR)/iso
KERNEL_ELF=$(BUILD_DIR)/prometheus-kernel.elf
KERNEL_BIN=$(BUILD_DIR)/prometheus-kernel.bin
GRUB_CFG=$(ISO_DIR)/boot/grub/grub.cfg

SRC_C=$(shell find kernel -name '*.c')
SRC_ASM64=$(shell find kernel -name '*.asm')
SRC_BOOT=$(shell find boot -name '*.asm')
OBJ_C=$(patsubst %.c,$(BUILD_DIR)/%.o,$(SRC_C))
OBJ_ASM64=$(patsubst %.asm,$(BUILD_DIR)/%.o,$(SRC_ASM64))
OBJ_BOOT32=$(patsubst %.asm,$(BUILD_DIR)/%.o,$(SRC_BOOT))

DEPFILES=$(OBJ_C:.o=.d)

.PHONY: all clean run debug iso

all: $(KERNEL_ELF)

-include $(DEPFILES)

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -Iinclude -c $< -o $@

$(BUILD_DIR)/kernel/%.o: kernel/%.asm
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/boot/%.o: boot/%.asm
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS32) $< -o $@

$(KERNEL_ELF): $(OBJ_BOOT32) $(OBJ_ASM64) $(OBJ_C) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJ_BOOT32) $(OBJ_ASM64) $(OBJ_C)

iso: $(KERNEL_ELF)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_ELF) $(ISO_DIR)/boot/kernel.elf
	echo 'set timeout=0' > $(GRUB_CFG)
	echo 'set default=0' >> $(GRUB_CFG)
	echo 'menuentry "Prometheus" {' >> $(GRUB_CFG)
	echo '  multiboot2 /boot/kernel.elf' >> $(GRUB_CFG)
	echo '  boot' >> $(GRUB_CFG)
	echo '}' >> $(GRUB_CFG)
	grub-mkrescue -o $(BUILD_DIR)/prometheus.iso $(ISO_DIR)

run: all
	qemu-system-x86_64 -kernel $(KERNEL_ELF) -serial stdio -no-reboot -no-shutdown

debug: all
	qemu-system-x86_64 -kernel $(KERNEL_ELF) -serial stdio -s -S

clean:
	rm -rf $(BUILD_DIR)

