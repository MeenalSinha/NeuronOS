// NeuronOS TCP/IP Stack Core
// net/net.c

#include "net.h"
#include "../kernel/kernel.h"
#include "../kernel/memory.h"

#include "../drivers/e1000.h"

#define MAX_SOCKETS 64

static net_socket_t sockets[MAX_SOCKETS];
static bool hardware_nic_active = false;

void net_init(void) {
    kernel_print("Initializing TCP/IP Stack with hardware bindings...\n", 0x0F);
    
    // Wire up the physical E1000 Driver if present
    // Let's assume PCI enumerator passed bus=0, slot=3, func=0 for the NIC
    e1000_init(0, 3, 0);
    hardware_nic_active = true;
    
    for (int i = 0; i < MAX_SOCKETS; i++) {
        sockets[i].socket_id = -1;
    }
}

int net_socket_create(void) {
    for (int i=0; i < MAX_SOCKETS; i++) {
        if (sockets[i].socket_id == -1) {
            sockets[i].socket_id = i;
            sockets[i].state = 0; // CLOSED
            return i;
        }
    }
    return -1;
}

bool net_socket_bind(int sockfd, ip_addr_t addr, uint16_t port) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS) return false;
    if (sockets[sockfd].socket_id != sockfd) return false;
    
    sockets[sockfd].local_ip = addr;
    sockets[sockfd].local_port = port;
    sockets[sockfd].state = 1; // BOUND
    return true;
}

bool net_socket_listen(int sockfd) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS) return false;
    if (sockets[sockfd].state != 1) return false;
    sockets[sockfd].state = 2; // LISTEN
    return true;
}

int net_socket_accept(int sockfd) {
    // Stub: Blocks and returns new client fd
    return -1;
}

bool net_connect(int sockfd, ip_addr_t remote_addr, uint16_t remote_port) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS) return false;
    sockets[sockfd].remote_ip = remote_addr;
    sockets[sockfd].remote_port = remote_port;
    
    // Simulate TCP Handshake over loopback
    if (remote_addr.ip[0] == 127 && remote_addr.ip[1] == 0 && remote_addr.ip[2] == 0 && remote_addr.ip[3] == 1) {
        sockets[sockfd].state = 3; // ESTABLISHED
        return true;
    }
    
    return false;
}

int net_send(int sockfd, const void* buffer, int len) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS) return -1;
    if (sockets[sockfd].state != 3) return -1;
    
    if (hardware_nic_active) {
        // Encap TCP/IP headers conceptually and ship to hardware ring buffer
        return e1000_send_packet(buffer, len);
    }
    return len; // Loopback pretend
}

int net_recv(int sockfd, void* buffer, int len) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS) return -1;
    if (sockets[sockfd].state != 3) return -1;
    
    if (hardware_nic_active) {
        // Poll hardware descriptor table for incoming frames
        return e1000_receive_packet(buffer); // Can block in physical system
    }
    return 0; 
}
