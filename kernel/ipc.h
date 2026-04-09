// NeuronOS Inter-Process Communication
// kernel/ipc.h

#ifndef IPC_H
#define IPC_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_MESSAGES 128
#define MSG_SIZE 256

typedef struct {
    uint32_t sender_pid;
    uint32_t type;
    uint8_t payload[MSG_SIZE];
} ipc_message_t;

void ipc_init(void);

// Message passing functions
bool ipc_send(uint32_t target_pid, uint32_t type, const void* payload, uint32_t size);
bool ipc_receive(uint32_t* sender_pid, uint32_t* type, void* payload_buffer);

// Shared Memory functions
void* shm_create(uint32_t key, uint32_t size);
void* shm_attach(uint32_t key);
bool shm_detach(void* shm_addr);

#endif // IPC_H
