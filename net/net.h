// NeuronOS TCP/IP Stack
// net/net.h

#ifndef NET_H
#define NET_H

#include <stdint.h>
#include <stdbool.h>

#define IPV4_LEN 4
#define MAC_LEN 6

typedef struct {
    uint8_t ip[IPV4_LEN];
} ip_addr_t;

typedef struct {
    uint8_t mac[MAC_LEN];
} mac_addr_t;

void net_init(void);

// Socket abstractions
typedef struct {
    int socket_id;
    ip_addr_t local_ip;
    uint16_t local_port;
    ip_addr_t remote_ip;
    uint16_t remote_port;
    int state;
} net_socket_t;

int net_socket_create(void);
bool net_socket_bind(int sockfd, ip_addr_t addr, uint16_t port);
bool net_socket_listen(int sockfd);
int net_socket_accept(int sockfd);
bool net_connect(int sockfd, ip_addr_t remote_addr, uint16_t remote_port);
int net_send(int sockfd, const void* buffer, int len);
int net_recv(int sockfd, void* buffer, int len);

#endif // NET_H
