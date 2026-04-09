// NeuronOS - xHCI (USB 3.0) Driver Header
// drivers/xhci.h

#ifndef XHCI_H
#define XHCI_H

#include <stdint.h>
#include <stdbool.h>

// xHCI Capability Registers
typedef struct __attribute__((packed)) {
    uint8_t  caplength;
    uint8_t  reserved;
    uint16_t hciversion;
    uint32_t hcsparams1;
    uint32_t hcsparams2;
    uint32_t hcsparams3;
    uint32_t hccparams1;
    uint32_t dboff;
    uint32_t rtsoff;
    uint32_t hccparams2;
} xhci_cap_regs_t;

// xHCI Operational Registers
typedef struct __attribute__((packed)) {
    uint32_t usbcmd;
    uint32_t usbsts;
    uint32_t pagesize;
    uint8_t  reserved1[8];
    uint32_t dnctrl;
    uint64_t crcr;
    uint8_t  reserved2[16];
    uint64_t dcbaap;
    uint32_t config;
} xhci_op_regs_t;

// xHCI Port Register Set
typedef struct __attribute__((packed)) {
    uint32_t portsc;
    uint32_t portpmsc;
    uint32_t portli;
    uint32_t porthlpmc;
} xhci_port_regs_t;

// TRB (Transfer Request Block) Structures
typedef struct __attribute__((packed)) {
    uint64_t parameter;
    uint32_t status;
    uint32_t control;
} xhci_trb_t;

// Driver API
void xhci_init(void *mmio_base);
void xhci_start_controller(void);
void xhci_handle_interrupt(void);

#endif // XHCI_H
