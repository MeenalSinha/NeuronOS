# Bootloader

The NeuronOS bootloader is a two-stage x86 bootloader that initializes the system and loads the kernel.

## Components

### Stage 1: boot.asm
- **Size:** 512 bytes (fits in MBR)
- **Purpose:** Load Stage 2 from disk
- **Location:** First sector of disk

**Key Functions:**
- BIOS bootstrap entry point
- Load Stage 2 into memory (0x7E00)
- Transfer control to Stage 2

### Stage 2: stage2.asm  
- **Size:** ~4KB
- **Purpose:** Prepare environment and load kernel
- **Location:** Sectors 2+

**Key Functions:**
- Detect memory using BIOS INT 0x15 (E820)
- **NEW:** Build system profile for AI engine initialization
  - Detect RAM size
  - Query CPU features (CPUID)
  - Capture boot timestamp
- Enable A20 line (access >1MB memory)
- Set up Global Descriptor Table (GDT)
- Switch to 32-bit protected mode
- Load kernel into memory
- Jump to kernel entry point

## System Profiling (AI-Native Feature)

Stage 2 builds a system profile that the AI engine uses to make boot-time decisions:

```asm
build_system_profile:
    ; Store at 0x7000 for kernel
    ; - Total RAM size
    ; - CPU features (from CPUID)
    ; - Boot timestamp
    ; - Profile marker (0x41495359 = 'AISY')
```

This allows AI to:
- Recommend scheduler configuration based on CPU cores
- Adjust memory thresholds based on available RAM
- Enable/disable features based on hardware capabilities

## Build

```bash
nasm -f bin boot.asm -o boot.bin
nasm -f bin stage2.asm -o stage2.bin
cat boot.bin stage2.bin > bootloader.bin
```

## Memory Layout

```
0x0000 - 0x0500 : BIOS Data Area
0x0500 - 0x7C00 : Free
0x7C00 - 0x7E00 : Stage 1 (MBR)
0x7E00 - 0x8E00 : Stage 2
0x7000 - 0x701F : System Profile (AI)
0x8000 - 0x9000 : Memory Map (E820)
0x00100000+     : Kernel
```

## Testing

```bash
# Create bootable image
make bootloader

# Test in QEMU
qemu-system-x86_64 -drive format=raw,file=bootloader.bin
```

## References

- Intel x86 Architecture Manual
- OSDev Wiki: Bootloader
- BIOS INT 0x15 E820 Memory Detection
