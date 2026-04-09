// NeuronOS AHCI Storage Driver
// drivers/ahci.h

#ifndef AHCI_H
#define AHCI_H

#include <stdint.h>
#include <stdbool.h>

#define SATA_SIG_ATA   0x00000101   // SATA drive
#define SATA_SIG_ATAPI 0xEB140101   // SATAPI drive

// AHCI initialization, to be called from PCI enumerator
void ahci_init(uint8_t bus, uint8_t slot, uint8_t func);

// Physical Region Descriptor Table (PRDT) entry
typedef struct {
    uint32_t dba;      // Data base address
    uint32_t dbau;     // Data base address upper 32 bits
    uint32_t rsv0;     // Reserved
    uint32_t dbc:22;   // Byte count, 4M max
    uint32_t rsv1:9;   // Reserved
    uint32_t i:1;      // Interrupt on completion
} __attribute__((packed)) ahci_prdt_entry_t;

// Command Table
typedef struct {
    uint8_t  cfis[64]; // Command FIS
    uint8_t  acmd[16]; // ATAPI command
    uint8_t  rsv[48];  // Reserved
    ahci_prdt_entry_t prdt[1]; // Physical region descriptor table entries
} __attribute__((packed)) ahci_command_table_t;

// Routine to send AHCI commands with DMA setups
bool ahci_read(uint32_t start_lba, uint32_t count, void* buffer);
bool ahci_write(uint32_t start_lba, uint32_t count, const void* buffer);

#endif // AHCI_H
