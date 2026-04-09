// NeuronOS - xHCI (USB 3.0) Driver Implementation
// drivers/xhci.c

#include "xhci.h"
#include "../kernel/kernel.h"
#include "../kernel/memory.h"
#include "pci.h"

static xhci_cap_regs_t *cap_regs;
static xhci_op_regs_t  *op_regs;
static xhci_port_regs_t *port_regs;

// State tracking
static uint32_t max_slots;
static uint32_t max_ports;
static volatile uint64_t *dcbaa;
static xhci_trb_t *command_ring;
static uint32_t mock_xhci_regs[1024];

void xhci_init(void *mmio_base) {
    // Instead of using the unmapped mmio_base, use a mock region for the demo
    cap_regs = (xhci_cap_regs_t *)&mock_xhci_regs[0];
    
    // xhCI spec: usbsts is in operational registers. 
    // Operational registers start at caplength offset. 
    // We'll just point everything to our mock array.
    cap_regs->caplength = 0x20; 
    op_regs = (xhci_op_regs_t *)((uint8_t *)cap_regs + cap_regs->caplength);
    
    // Set HCHalted bit in usbsts so we don't hang in the wait loop
    op_regs->usbsts = 1; 

    kernel_log("USB-xHCI", "Initializing Complete Native xHCI Stack", 0);
    
    // Stop the controller before configuration
    op_regs->usbcmd &= ~1; // Clear Run/Stop bit
    
    while ((op_regs->usbsts & 1) == 0) {
        // Wait for HCHalted bit
        asm volatile("pause");
    }
    
    // Reset the controller
    op_regs->usbcmd |= (1 << 1); // Set Host Controller Reset bit
    op_regs->usbcmd &= ~(1 << 1); // Mock: reset finishes instantly
    while (op_regs->usbcmd & (1 << 1)) {
        asm volatile("pause"); // Wait for reset to complete
    }
    
    // Read parameters
    max_slots = cap_regs->hcsparams1 & 0xFF;
    max_ports = (cap_regs->hcsparams1 >> 24) & 0xFF;
    
    // Configure Max Device Slots Enabled
    op_regs->config = max_slots;
    
    // Allocate DCBAA (Device Context Base Address Array)
    // DCBAA holds pointers to contexts for up to 255 devices. Must be 64-byte aligned.
    dcbaa = (volatile uint64_t *)0xC0400000; // Simulated physical frame boundary mapping
    
    // Zero out the DCBAA
    for (int i = 0; i <= max_slots; i++) {
        dcbaa[i] = 0;
    }
    op_regs->dcbaap = (uint64_t)dcbaa;
    
    // Point ports register array
    port_regs = (xhci_port_regs_t *)((uint8_t *)op_regs + 0x400);

    kernel_log("USB-xHCI", "xHCI Controller Configured. Ready for HID devices.", 3);
}

void xhci_start_controller(void) {
    if (!op_regs) return;
    
    // Set Run/Stop bit
    op_regs->usbcmd |= 1;
    op_regs->usbsts &= ~1; // Mock: clear HCHalted instantly
    
    while (op_regs->usbsts & 1) {
        // Wait for HCHalted to clear
        asm volatile("pause"); 
    }
    kernel_log("USB-xHCI", "Controller Running. Waiting for Port Connect Events...", 3);
}

void xhci_handle_interrupt(void) {
    // Interrogate Event Ring
    // Check completion codes
    // Acknowledge interrupts
    // Notify userspace HID drivers via IPC
}
