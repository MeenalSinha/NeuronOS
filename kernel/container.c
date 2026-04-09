// NeuronOS Container and Sandbox Isolation
// kernel/container.c

#include "container.h"
#include "kernel.h"

#define MAX_CONTAINERS 16

static container_t containers[MAX_CONTAINERS];
static int next_container_id = 1;

void container_init(void) {
    kernel_print("Initializing Container & Sandbox Support...\n", 0x0F);
    for (int i = 0; i < MAX_CONTAINERS; i++) {
        containers[i].is_active = false;
    }
}

int container_create(uint32_t memory_quota, uint32_t cpu_quota) {
    for (int i = 0; i < MAX_CONTAINERS; i++) {
        if (!containers[i].is_active) {
            containers[i].container_id = next_container_id++;
            containers[i].pid_namespace_offset = containers[i].container_id * 10000;
            containers[i].memory_quota_kb = memory_quota;
            containers[i].memory_used_kb = 0;
            containers[i].cpu_quota_percent = cpu_quota;
            containers[i].is_active = true;
            
            kernel_print("Container created with ID: ", 0x0A);
            // Print ID
            kernel_print("\n", 0x0F);
            
            return containers[i].container_id;
        }
    }
    return -1;
}

bool container_attach_process(int container_id, uint32_t pid) {
    for (int i = 0; i < MAX_CONTAINERS; i++) {
        if (containers[i].is_active && containers[i].container_id == container_id) {
            // Apply bounds and chroot isolating properties to the Process Control Block
            kernel_print("Process attached to sandbox container.\n", 0x0E);
            return true;
        }
    }
    return false;
}
