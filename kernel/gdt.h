// NeuronOS Global Descriptor Table and TSS
// kernel/gdt.h

#ifndef GDT_H
#define GDT_H

#include <stdint.h>

// Defines a GDT entry
struct gdt_entry_struct {
    uint16_t limit_low;           // The lower 16 bits of the limit
    uint16_t base_low;            // The lower 16 bits of the base
    uint8_t  base_middle;         // The next 8 bits of the base
    uint8_t  access;              // Access flags
    uint8_t  granularity;         // Granularity and limit upper 4 bits
    uint8_t  base_high;           // The last 8 bits of the base
} __attribute__((packed));
typedef struct gdt_entry_struct gdt_entry_t;

// Pointer to GDT
struct gdt_ptr_struct {
    uint16_t limit;               // The upper 16 bits of all selector limits
    uint32_t base;                // The address of the first gdt_entry_t struct
} __attribute__((packed));
typedef struct gdt_ptr_struct gdt_ptr_t;

// Task State Segment
struct tss_entry_struct {
    uint32_t prev_tss;
    uint32_t esp0;       // Stack pointer to load when changing to kernel mode
    uint32_t ss0;        // Stack segment to load when changing to kernel mode
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));
typedef struct tss_entry_struct tss_entry_t;

// Initialize GDT and TSS
void init_gdt(void);

// Set the kernel stack in TSS for when a user process interrupts
void set_kernel_stack(uint32_t stack);

#endif // GDT_H
