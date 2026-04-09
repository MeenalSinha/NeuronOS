// NeuronOS Basic PCI Enumerator
// drivers/pci.c

#include "pci.h"
#include "../kernel/kernel.h"

static inline void outl(uint16_t port, uint32_t val) {
    asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;

    // Create configuration address as per PCI standard
    address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    
    outl(0xCF8, address);
    return inl(0xCFC);
}

void pci_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(0xCF8, address);
    outl(0xCFC, val);
}

uint32_t pci_get_bar(uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar_index) {
    uint8_t offset = 0x10 + (bar_index * 4);
    uint32_t bar_val = pci_read_config(bus, slot, func, offset);
    
    // Mask off the type bits (simplified for 32-bit MMIO)
    return bar_val & 0xFFFFFFF0;
}

void pci_init(void) {
    kernel_print("Initializing PCI Bus...\n", 0x0F);
    
    // Simplistic enumeration
    for(uint16_t bus = 0; bus < 256; bus++) {
        for(uint8_t slot = 0; slot < 32; slot++) {
            uint32_t vendor_device = pci_read_config(bus, slot, 0, 0);
            if((vendor_device & 0xFFFF) != 0xFFFF) {
                // Determine if this is an AHCI Controller (Class 0x01, Subclass 0x06)
                uint32_t class_info = pci_read_config(bus, slot, 0, 0x08);
                uint8_t class_code = (class_info >> 24) & 0xFF;
                uint8_t subclass = (class_info >> 16) & 0xFF;
                
                if (class_code == 0x01 && subclass == 0x06) {
                    kernel_print("Found AHCI Controller!\n", 0x0A);
                    
                    // In a real OS we would instantiate the AHCI driver here
                    // ahci_init(bus, slot, 0);
                }
            }
        }
    }
}
