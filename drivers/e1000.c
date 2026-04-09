// NeuronOS Intel E1000 Gigabit Ethernet Driver
// drivers/e1000.c

#include "e1000.h"
#include "pci.h"
#include "../kernel/kernel.h"
#include "../kernel/memory.h"

static uint8_t e1000_mac[6];
static uint32_t mmio_base;

static struct e1000_rx_desc *rx_descs;
static struct e1000_tx_desc *tx_descs;
static uint16_t rx_cur;
static uint16_t tx_cur;

static void write_command(uint16_t p_address, uint32_t p_value) {
    *(volatile uint32_t *)((uint32_t)mmio_base + p_address) = p_value;
}

static uint32_t read_command(uint16_t p_address) {
    return *(volatile uint32_t *)((uint32_t)mmio_base + p_address);
}

void e1000_init(uint8_t bus, uint8_t slot, uint8_t func) {
    kernel_print("Initializing Intel E1000 NIC...\n", 0x0F);
    
    // Obtain MMIO Address (BAR 0)
    mmio_base = pci_get_bar(bus, slot, func, 0) & 0xFFFFFFF0;
    if (mmio_base == 0 || mmio_base == 0xFFFFFFF0) {
        kernel_log("E1000", "NIC not found at PCI 0:3:0", 1);
        return;
    }

    // Map the MMIO region (assuming 128KB is enough)
    for (uint32_t i = 0; i < 32; i++) {
        map_page(get_current_page_directory(), mmio_base + i * 4096, mmio_base + i * 4096, 0x3);
    }
    
    // Read MAC address (simplified from EEPROM)
    uint32_t d1 = read_command(0x5400); // RAL
    uint32_t d2 = read_command(0x5404); // RAH
    e1000_mac[0] = d1 & 0xFF;
    e1000_mac[1] = (d1 >> 8) & 0xFF;
    e1000_mac[2] = (d1 >> 16) & 0xFF;
    e1000_mac[3] = (d1 >> 24) & 0xFF;
    e1000_mac[4] = d2 & 0xFF;
    e1000_mac[5] = (d2 >> 8) & 0xFF;
    
    // Physical Setup (Requires proper memory layout in real environment)
    uint32_t rx_phys_mem = 0x500000;
    rx_descs = (struct e1000_rx_desc *)rx_phys_mem;
    for(int i = 0; i < E1000_RX_DESCRIPTORS; i++) {
        rx_descs[i].addr = rx_phys_mem + 0x1000 + (i * 8192); // Assign buffers
        rx_descs[i].status = 0;
    }
    
    uint32_t tx_phys_mem = 0x600000;
    tx_descs = (struct e1000_tx_desc *)tx_phys_mem;
    for(int i = 0; i < E1000_TX_DESCRIPTORS; i++) {
        tx_descs[i].addr = 0;
        tx_descs[i].cmd = 0;
        tx_descs[i].status = 1; // Descriptor Done
    }
    
    // Initialize RX and TX Registers on Hardware
    write_command(E1000_REG_RXDESCLO, (uint32_t)rx_descs);
    write_command(E1000_REG_RXDESCHI, 0);
    write_command(E1000_REG_RXDESCLEN, E1000_RX_DESCRIPTORS * 16);
    write_command(E1000_REG_RXDESCHW, 0);
    write_command(E1000_REG_RXDESCTAIL, E1000_RX_DESCRIPTORS - 1);
    
    write_command(E1000_REG_TXDESCLO, (uint32_t)tx_descs);
    write_command(E1000_REG_TXDESCHI, 0);
    write_command(E1000_REG_TXDESCLEN, E1000_TX_DESCRIPTORS * 16);
    write_command(E1000_REG_TXDESCHW, 0);
    write_command(E1000_REG_TXDESCTAIL, 0);
    
    // Enable Receiver & Transmitter
    write_command(E1000_REG_RCTRL, 0x8002); // Enable, BSIZE=2048
    write_command(E1000_REG_TCTRL, 0x010400FA); // Enable, EN, PSP
    
    kernel_print("E1000 NIC Configured and Ready!\n", 0x0A);
}

int e1000_send_packet(const void *data, uint16_t len) {
    tx_descs[tx_cur].addr = (uint32_t)data; // Send buffer
    tx_descs[tx_cur].length = len;
    tx_descs[tx_cur].cmd = (1 << 3) | (1 << 0); // RS=1, EOP=1
    tx_descs[tx_cur].status = 0;
    
    uint16_t old_cur = tx_cur;
    tx_cur = (tx_cur + 1) % E1000_TX_DESCRIPTORS;
    write_command(E1000_REG_TXDESCTAIL, tx_cur);
    
    // Wait for packet copy
    // while(!(tx_descs[old_cur].status & 0xff));
    return len;
}

int e1000_receive_packet(void *buffer) {
    if ((rx_descs[rx_cur].status & 0x01) == 0) return 0; // Descriptor Done bit
    
    uint16_t len = rx_descs[rx_cur].length;
    // Copy out from rx_descs[rx_cur].addr to buffer
    memcpy(buffer, (void*)((uint32_t)rx_descs[rx_cur].addr), len);
    
    rx_descs[rx_cur].status = 0;
    uint16_t old_cur = rx_cur;
    rx_cur = (rx_cur + 1) % E1000_RX_DESCRIPTORS;
    write_command(E1000_REG_RXDESCTAIL, old_cur); // Update tail pointer
    
    return len;
}

uint8_t* e1000_get_mac_address(void) {
    return e1000_mac;
}
