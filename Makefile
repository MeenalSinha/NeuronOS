# NeuronOS Makefile — Production Build System
# Targets:  all | kernel | iso | run | run-serial | debug | clean

# ── Toolchain ──────────────────────────────────────────────────────────────────
AS     = nasm
CC     = gcc
LD     = ld

# ── Flags ──────────────────────────────────────────────────────────────────────
# -m32          : 32-bit code (Multiboot + our GDT/IDT are 32-bit)
# -ffreestanding: no libc, no start files
# -nostdlib     : don't link against libc
# -fno-pie      : no position-independent code (breaks bare-metal linking)
# -fno-stack-protector: no __stack_chk_fail (no libc)
# -std=gnu99    : C99 with GNU extensions
CFLAGS = -m32 -std=gnu99 -ffreestanding -nostdlib -fno-pie \
         -fno-stack-protector -fno-builtin \
         -mno-sse -mno-mmx -mno-sse2 \
         -Wall -Wextra -Wno-unused-parameter \
         -I./kernel -I./kernel/ai -I./drivers -I./fs -I./net \
         -O2

ASFLAGS_BIN = -f bin
ASFLAGS_ELF = -f elf32

LDFLAGS = -m elf_i386 -T linker.ld --oformat elf32-i386

# ── Directories ────────────────────────────────────────────────────────────────
BUILD_DIR    = build
ISO_DIR      = $(BUILD_DIR)/iso
ISOBOOT_DIR  = $(ISO_DIR)/boot
GRUB_DIR     = $(ISOBOOT_DIR)/grub

# ── Output files ───────────────────────────────────────────────────────────────
KERNEL  = $(BUILD_DIR)/kernel.bin
ISO     = $(BUILD_DIR)/neuron-os.iso

# ── Source discovery ───────────────────────────────────────────────────────────
KERNEL_C_SRCS = \
    kernel/klibc.c \
    kernel/timer.c \
    kernel/main.c \
    kernel/memory.c \
    kernel/process.c \
    kernel/scheduler.c \
    kernel/gdt.c \
    kernel/idt.c \
    kernel/ipc.c \
    kernel/security.c \
    kernel/healing.c \
    kernel/tracing.c \
    kernel/container.c \
    kernel/module.c \
    kernel/smp.c \
    kernel/syscall.c \
    kernel/metrics.c \
    kernel/ai/ai_engine.c \
    kernel/ai/decision_tree.c \
    kernel/ai/gpu_backend.c \
    drivers/pci.c \
    drivers/ahci.c \
    drivers/e1000.c \
    drivers/xhci.c \
    fs/neuronfs.c \
    net/net.c \
    userspace/shell.c

KERNEL_C_OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(KERNEL_C_SRCS))
ENTRY_OBJ     = $(BUILD_DIR)/kernel/arch/entry.o
ARCH_ASM_OBJ  = $(BUILD_DIR)/kernel/arch_asm.o

# ── Phony targets ──────────────────────────────────────────────────────────────
.PHONY: all kernel iso run run-serial debug clean help

all: iso

# ── Kernel entry (ASM) ────────────────────────────────────────────────────────
$(ENTRY_OBJ): kernel/arch/entry.asm
	@mkdir -p $(dir $@)
	@echo "[ASM]  $<"
	$(AS) $(ASFLAGS_ELF) $< -o $@

$(ARCH_ASM_OBJ): kernel/arch_asm.asm
	@mkdir -p $(dir $@)
	@echo "[ASM]  $<"
	$(AS) $(ASFLAGS_ELF) $< -o $@

# ── C object compilation ──────────────────────────────────────────────────────
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "[CC]   $<"
	$(CC) $(CFLAGS) -c $< -o $@

LIBGCC := $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)

# ── Kernel link ───────────────────────────────────────────────────────────────
$(KERNEL): $(ENTRY_OBJ) $(ARCH_ASM_OBJ) $(KERNEL_C_OBJS)
	@echo "[LD]   Linking kernel..."
	$(LD) $(LDFLAGS) -o $@ $(ENTRY_OBJ) $(ARCH_ASM_OBJ) $(KERNEL_C_OBJS) $(LIBGCC)
	@echo "[OK]   Kernel: $@  ($$(du -h $@ | cut -f1))"

kernel: $(KERNEL)

# ── GRUB ISO ──────────────────────────────────────────────────────────────────
$(ISO): $(KERNEL)
	@echo "[ISO]  Building bootable ISO..."
	@mkdir -p $(GRUB_DIR)
	@cp $(KERNEL) $(ISOBOOT_DIR)/kernel.bin
	@printf 'set timeout=2\nset default=0\n\nmenuentry "NeuronOS v1.0 (AI-Native)" {\n    multiboot /boot/kernel.bin\n    boot\n}\n' > $(GRUB_DIR)/grub.cfg
	@grub-mkrescue -o $@ $(ISO_DIR) 2>/dev/null
	@echo "[OK]   ISO: $@  ($$(du -h $@ | cut -f1))"

iso: $(ISO)

# ── Run (with display — use outside Docker) ───────────────────────────────────
run: $(ISO)
	@echo "[QEMU] Starting NeuronOS (display mode)..."
	qemu-system-i386 \
	    -cdrom $(ISO) \
	    -m 256M \
	    -serial stdio \
	    -no-reboot \
	    -no-shutdown \
	    -vga std

# ── Run (serial only — safe inside Docker / CI) ───────────────────────────────
run-serial: $(ISO)
	@echo "[QEMU] Starting NeuronOS (serial-only, no display)..."
	qemu-system-i386 \
	    -cdrom $(ISO) \
	    -m 256M \
	    -nographic \
	    -serial mon:stdio \
	    -no-reboot \
	    -no-shutdown

# ── Debug (GDB stub on port 1234) ─────────────────────────────────────────────
debug: $(ISO)
	@echo "[QEMU] Debug mode — attach GDB to localhost:1234"
	qemu-system-i386 \
	    -cdrom $(ISO) \
	    -m 256M \
	    -s -S \
	    -serial stdio \
	    -nographic

# ── Clean ─────────────────────────────────────────────────────────────────────
clean:
	@echo "[CLN]  Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@echo "[OK]   Clean complete"

# ── Help ──────────────────────────────────────────────────────────────────────
help:
	@echo ""
	@echo "NeuronOS Build System"
	@echo "====================="
	@echo "  make              - Build bootable ISO (default)"
	@echo "  make kernel       - Compile kernel binary only"
	@echo "  make iso          - Build full bootable ISO"
	@echo "  make run          - Run in QEMU with display"
	@echo "  make run-serial   - Run in QEMU terminal-only (Docker safe)"
	@echo "  make debug        - Run with GDB stub on :1234"
	@echo "  make clean        - Remove build directory"
	@echo ""
	@echo "Docker workflow:"
	@echo "  docker build -t neuronos ."
	@echo "  docker run --rm -it neuronos"
	@echo ""
