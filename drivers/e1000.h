// NeuronOS Intel E1000 Gigabit Ethernet Driver
// drivers/e1000.h

#ifndef E1000_H
#define E1000_H

#include <stdint.h>
#include <stdbool.h>

#define E1000_VENDOR_ID 0x8086
#define E1000_DEVICE_ID 0x100E

// Register offsets
#define E1000_REG_CTRL     0x0000
#define E1000_REG_STATUS   0x0008
#define E1000_REG_EEPROM   0x0014
#define E1000_REG_ICR      0x00C0
#define E1000_REG_RCTRL    0x0100
#define E1000_REG_RXDESCLO 0x2800
#define E1000_REG_RXDESCHI 0x2804
#define E1000_REG_RXDESCLEN 0x2808
#define E1000_REG_RXDESCHW 0x2810
#define E1000_REG_RXDESCTAIL 0x2818
#define E1000_REG_TCTRL    0x0400
#define E1000_REG_TXDESCLO 0x3800
#define E1000_REG_TXDESCHI 0x3804
#define E1000_REG_TXDESCLEN 0x3808
#define E1000_REG_TXDESCHW 0x3810
#define E1000_REG_TXDESCTAIL 0x3818
#define E1000_REG_MTA      0x5200

#define E1000_RX_DESCRIPTORS 32
#define E1000_TX_DESCRIPTORS 8

// Receive Descriptor
struct e1000_rx_desc {
    uint64_t addr;      // Address of the descriptor's data buffer
    uint16_t length;    // Length of data DMAed into data buffer
    uint16_t csum;      // Packet checksum
    uint8_t status;     // Descriptor status
    uint8_t errors;     // Descriptor Errors
    uint16_t special;
} __attribute__((packed));

// Transmit Descriptor
struct e1000_tx_desc {
    uint64_t addr;      // Address of the descriptor's data buffer
    uint16_t length;    // Data buffer length
    uint8_t cso;        // Checksum offset
    uint8_t cmd;        // Descriptor control
    uint8_t status;     // Descriptor status
    uint8_t css;        // Checksum start
    uint16_t special;
} __attribute__((packed));

void e1000_init(uint8_t bus, uint8_t slot, uint8_t func);
int e1000_send_packet(const void *data, uint16_t len);
int e1000_receive_packet(void *buffer);

uint8_t* e1000_get_mac_address(void);

#endif // E1000_H
