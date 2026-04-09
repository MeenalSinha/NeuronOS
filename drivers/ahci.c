// NeuronOS AHCI Storage Driver
// drivers/ahci.c

#include "ahci.h"
#include "pci.h"
#include "../kernel/kernel.h"

// Simplified AHCI Memory-Mapped Registers Space (ABAR)
typedef struct {
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t cmd;
    uint32_t resv[26];
    // Followed by port structures
} __attribute__((packed)) ahci_hba_mem_t;

typedef struct {
    uint32_t clb;
    uint32_t clbu;
    uint32_t fb;
    uint32_t fbu;
    uint32_t is;
    uint32_t ie;
    uint32_t cmd;
    uint32_t rsv0;
    uint32_t tfd;
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;
    uint32_t rsv1[11];
    uint32_t vendor[4];
} __attribute__((packed)) ahci_port_t;

typedef struct {
    uint8_t  cfl:5;
    uint8_t  a:1;
    uint8_t  w:1;
    uint8_t  p:1;
    uint8_t  r:1;
    uint8_t  b:1;
    uint8_t  c:1;
    uint8_t  rsv0:1;
    uint8_t  pmp:4;
    uint16_t prdtl;
    uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t rsv1[4];
} __attribute__((packed)) ahci_cmd_header_t;

// Set up DMA structure memory dynamically linked to the controller
static int find_cmd_slot(ahci_port_t *port) {
    uint32_t slots = (port->sact | port->ci);
    for (int i=0; i<32; i++) {
        if ((slots & (1 << i)) == 0) return i;
    }
    return -1;
}

static ahci_hba_mem_t* abar = NULL;

static ahci_port_t* active_port = NULL;

void ahci_init(uint8_t bus, uint8_t slot, uint8_t func) {
    uint32_t bar5 = pci_get_bar(bus, slot, func, 5);
    abar = (ahci_hba_mem_t*)bar5;
    
    // In a real OS you must page-map the BAR5 physical address before dereferencing
    kernel_print("AHCI Base Address at: 0x", 0x0F);
    // would print hex here
    kernel_print("\n", 0x0F);
    
    // Simplistic discovery: find the first active port with a SATA drive attached
    uint32_t pi = abar->pi;
    for(int i = 0; i < 32; i++) {
        if (pi & 1) {
            ahci_port_t* port = (ahci_port_t*)((uint32_t)abar + 0x100 + (i * 0x80));
            uint32_t ssts = port->ssts;
            uint8_t ipm = (ssts >> 8) & 0x0F;
            uint8_t det = ssts & 0x0F;
            
            if (det == 3 && ipm == 1 && port->sig == SATA_SIG_ATA) {
                kernel_print("Found SATA Drive on AHCI Port!\n", 0x0A);
                active_port = port;
                
                // Hardware DMA Setup: Allocate contiguous physical page for Port structures
                // (In a true physical OS, this requires `alloc_frame` natively addressing < 4GB logic)
                uint32_t ahci_phys_base = 0x400000; // Simulated safe <4G address
                port->clb = ahci_phys_base;
                port->fb  = ahci_phys_base + (32 * sizeof(ahci_cmd_header_t)); 
                
                // Set Command Table points
                ahci_cmd_header_t *cmdheader = (ahci_cmd_header_t*)(port->clb);
                for (int i=0; i<32; i++) {
                    cmdheader[i].prdtl = 1; // 1 PRDT entry per command
                    cmdheader[i].ctba = ahci_phys_base + 0x1000 + (0x100 * i);
                }
                
                break;
            }
        }
        pi >>= 1;
    }
}

// Minimal stub logic indicating how read is submitted to HBA
bool ahci_read(uint32_t start_lba, uint32_t count, void* buffer) {
    if (!active_port) return false;
    
    // 1. clear PxIS
    active_port->is = (uint32_t)-1;
    
    // 2. Setup PRDT to point to buffer
    int slot = find_cmd_slot(active_port);
    if (slot == -1) return false;
    
    ahci_cmd_header_t *cmdheader = (ahci_cmd_header_t*)active_port->clb;
    cmdheader += slot;
    cmdheader->cfl = sizeof(uint32_t) * 5 / sizeof(uint32_t); // Command FIS size
    cmdheader->w = 0; // Read from device
    cmdheader->prdtl = 1; // PRDT entries count
    
    ahci_command_table_t *cmdtbl = (ahci_command_table_t*)(cmdheader->ctba);
    // memset(cmdtbl, 0, sizeof(ahci_command_table_t));
    
    // Memory map the physical translation
    cmdtbl->prdt[0].dba = (uint32_t)buffer;
    cmdtbl->prdt[0].dbc = (count * 512) - 1; 
    cmdtbl->prdt[0].i = 1;
    
    // 3. Setup Command FIS (0x25 READ DMA EXT)
    cmdtbl->cfis[0] = 0x27;  // FIS type
    cmdtbl->cfis[1] = 0x80;  // Update cmd reg
    cmdtbl->cfis[2] = 0x25;  // READ DMA EXT
    cmdtbl->cfis[4] = start_lba & 0xFF;
    cmdtbl->cfis[5] = (start_lba >> 8) & 0xFF;
    cmdtbl->cfis[6] = (start_lba >> 16) & 0xFF;
    cmdtbl->cfis[7] = 0x40;  // LBA mode
    
    // 4. Issue command by setting PxCI bit
    active_port->ci = 1 << slot;
    
    kernel_print("AHCI READ Command dispatched via hardware DMA PRDT.\n", 0x0A);
    // Wait for completion
    // while (active_port->ci & (1 << slot));
    
    return true;
}

// Minimal stub logic indicating how write is submitted to HBA
bool ahci_write(uint32_t start_lba, uint32_t count, const void* buffer) {
    if (!active_port) return false;
    
    active_port->is = (uint32_t)-1;
    int slot = find_cmd_slot(active_port);
    if (slot == -1) return false;
    
    ahci_cmd_header_t *cmdheader = (ahci_cmd_header_t*)active_port->clb;
    cmdheader += slot;
    cmdheader->cfl = sizeof(uint32_t) * 5 / sizeof(uint32_t);
    cmdheader->w = 1; // Write to device
    cmdheader->prdtl = 1;
    
    ahci_command_table_t *cmdtbl = (ahci_command_table_t*)(cmdheader->ctba);
    
    cmdtbl->prdt[0].dba = (uint32_t)buffer;
    cmdtbl->prdt[0].dbc = (count * 512) - 1; 
    cmdtbl->prdt[0].i = 1;
    
    cmdtbl->cfis[0] = 0x27;
    cmdtbl->cfis[1] = 0x80;
    cmdtbl->cfis[2] = 0x35;  // WRITE DMA EXT
    cmdtbl->cfis[4] = start_lba & 0xFF;
    cmdtbl->cfis[5] = (start_lba >> 8) & 0xFF;
    cmdtbl->cfis[6] = (start_lba >> 16) & 0xFF;
    cmdtbl->cfis[7] = 0x40;  // LBA mode
    
    active_port->ci = 1 << slot;
    kernel_print("AHCI WRITE Command dispatched via hardware DMA PRDT.\n", 0x0A);
    
    return true;
}
