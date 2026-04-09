// NeuronOS Container and Sandbox Isolation
// kernel/container.h

#ifndef CONTAINER_H
#define CONTAINER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t container_id;
    uint32_t pid_namespace_offset; 
    uint32_t memory_quota_kb;
    uint32_t memory_used_kb;
    uint32_t cpu_quota_percent;
    bool is_active;
} container_t;

void container_init(void);

// Create a new sandbox environment
int container_create(uint32_t memory_quota, uint32_t cpu_quota);

// Migrate an existing PID into a container sandbox
bool container_attach_process(int container_id, uint32_t pid);

#endif // CONTAINER_H
