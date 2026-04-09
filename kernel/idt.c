// NeuronOS Interrupt Descriptor Table
// kernel/idt.c

#include "idt.h"
#include "memory.h"

idt_entry_t idt_entries[256];
idt_ptr_t   idt_ptr;

extern void idt_flush(uint32_t);
extern void isr_stub_handler(void);

static void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt_entries[num].base_lo = base & 0xFFFF;
    idt_entries[num].base_hi = (base >> 16) & 0xFFFF;
    idt_entries[num].sel     = sel;
    idt_entries[num].always0 = 0;
    // We must uncomment the OR below when we get to using user-mode.
    // It sets the interrupt gate's privilege level to 3.
    idt_entries[num].flags   = flags /* | 0x60 */;
}

void init_idt(void) {
    idt_ptr.limit = sizeof(idt_entry_t) * 256 - 1;
    idt_ptr.base  = (uint32_t)&idt_entries;

    memset(&idt_entries, 0, sizeof(idt_entry_t)*256);

    // Setup basic exceptions & syscall (0x80)
    // Normally you'd register 256 explicit stubs, simplify to 1 stub for demonstration
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, (uint32_t)isr_stub_handler, 0x08, 0x8E);
    }
    
    // Syscall gate explicitly needs DPL 3
    idt_set_gate(0x80, (uint32_t)isr_stub_handler, 0x08, 0xEE);

    idt_flush((uint32_t)&idt_ptr);
}
