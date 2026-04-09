// NeuronOS Basic PCI Enumerator
// drivers/pci.h

#ifndef PCI_H
#define PCI_H

#include <stdint.h>

void pci_init(void);
uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);

// Retrieve memory mapped base address from BAR (Base Address Register)
uint32_t pci_get_bar(uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar_index);

#endif // PCI_H
