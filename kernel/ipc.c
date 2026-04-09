// NeuronOS Inter-Process Communication
// kernel/ipc.c

#include "ipc.h"
#include "kernel.h"
#include "memory.h"

// Ring buffer for IPC messages per process
typedef struct {
    ipc_message_t messages[MAX_MESSAGES];
    int head;
    int tail;
} msg_queue_t;

static msg_queue_t msg_queues[256];

// Shared memory allocations
typedef struct {
    uint32_t key;
    void* physical_addr;
    uint32_t size;
    int ref_count;
} shm_block_t;

static shm_block_t shm_blocks[64];

void ipc_init(void) {
    kernel_print("Initializing Inter-Process Communication...\n", 0x0F);
    for (int i = 0; i < 256; i++) {
        msg_queues[i].head = 0;
        msg_queues[i].tail = 0;
    }
    
    for(int i = 0; i < 64; i++) {
        shm_blocks[i].key = 0;
        shm_blocks[i].ref_count = 0;
    }
}

bool ipc_send(uint32_t target_pid, uint32_t type, const void* payload, uint32_t size) {
    if (target_pid >= 256) return false;
    
    msg_queue_t* queue = &msg_queues[target_pid];
    int next_head = (queue->head + 1) % MAX_MESSAGES;
    
    // Queue full
    if (next_head == queue->tail) return false;
    
    ipc_message_t* msg = &queue->messages[queue->head];
    msg->sender_pid = 0; // In reality, fetch from current process block
    msg->type = type;
    
    if (size > MSG_SIZE) size = MSG_SIZE;
    memcpy(msg->payload, payload, size);
    
    queue->head = next_head;
    return true;
}

bool ipc_receive(uint32_t* sender_pid, uint32_t* type, void* payload_buffer) {
    uint32_t current_pid = 0; // Fetch from current process in production
    msg_queue_t* queue = &msg_queues[current_pid];
    
    // Empty queue
    if (queue->head == queue->tail) return false;
    
    ipc_message_t* msg = &queue->messages[queue->tail];
    *sender_pid = msg->sender_pid;
    *type = msg->type;
    memcpy(payload_buffer, msg->payload, MSG_SIZE);
    
    queue->tail = (queue->tail + 1) % MAX_MESSAGES;
    return true;
}

void* shm_create(uint32_t key, uint32_t size) {
    for (int i=0; i<64; i++) {
        if (shm_blocks[i].key == 0) {
            shm_blocks[i].key = key;
            shm_blocks[i].size = size;
            shm_blocks[i].physical_addr = kmalloc(size); // Maps physical memory globally
            shm_blocks[i].ref_count = 1;
            return shm_blocks[i].physical_addr;
        }
    }
    return NULL;
}

void* shm_attach(uint32_t key) {
    for (int i=0; i<64; i++) {
        if (shm_blocks[i].key == key && shm_blocks[i].ref_count > 0) {
            shm_blocks[i].ref_count++;
            
            // In a real Virtual Memory System, we map shm_blocks[i].physical_addr 
            // into the current process's Page Directory here.
            return shm_blocks[i].physical_addr;
        }
    }
    return NULL;
}

bool shm_detach(void* shm_addr) {
    for (int i=0; i<64; i++) {
        if (shm_blocks[i].physical_addr == shm_addr) {
            shm_blocks[i].ref_count--;
            if (shm_blocks[i].ref_count <= 0) {
                kfree(shm_blocks[i].physical_addr);
                shm_blocks[i].key = 0;
            }
            return true;
        }
    }
    return false;
}
