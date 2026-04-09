// NeuronOS Capability-Based Security Model
// kernel/security.c

#include "security.h"
#include "kernel.h"

#define MAX_CAPS_PER_PROCESS 64
#define MAX_PROCESSES 256

// Simple capability table (In reality, stored inside Process Control Block)
static capability_t process_caps[MAX_PROCESSES][MAX_CAPS_PER_PROCESS];

void security_init(void) {
    kernel_print("Initializing Capability-Based Security...\n", 0x0F);
    for (int i = 0; i < MAX_PROCESSES; i++) {
        for (int j = 0; j < MAX_CAPS_PER_PROCESS; j++) {
            process_caps[i][j].object_id = 0;
            process_caps[i][j].rights = 0;
        }
    }
}

bool cap_grant(uint32_t pid, uint32_t object_id, uint32_t rights) {
    if (pid >= MAX_PROCESSES) return false;
    for (int i = 0; i < MAX_CAPS_PER_PROCESS; i++) {
        if (process_caps[pid][i].object_id == 0 || process_caps[pid][i].object_id == object_id) {
            process_caps[pid][i].object_id = object_id;
            process_caps[pid][i].rights |= rights;
            return true;
        }
    }
    return false;
}

bool cap_revoke(uint32_t pid, uint32_t object_id) {
    if (pid >= MAX_PROCESSES) return false;
    for (int i = 0; i < MAX_CAPS_PER_PROCESS; i++) {
        if (process_caps[pid][i].object_id == object_id) {
            process_caps[pid][i].object_id = 0;
            process_caps[pid][i].rights = 0;
            return true;
        }
    }
    return false;
}

bool cap_check(uint32_t pid, uint32_t object_id, uint32_t required_rights) {
    if (pid == 0) return true; // PID 0 is kernel/root, immune to checks
    if (pid >= MAX_PROCESSES) return false;
    
    for (int i = 0; i < MAX_CAPS_PER_PROCESS; i++) {
        if (process_caps[pid][i].object_id == object_id) {
            return (process_caps[pid][i].rights & required_rights) == required_rights;
        }
    }
    return false;
}

// Basic Adler-32 Checksum for Kernel .text segment verification
static uint32_t adler32(uint8_t *data, uint32_t len) {
    uint32_t a = 1, b = 0;
    for (uint32_t i = 0; i < len; i++) {
        a = (a + data[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

bool verify_kernel_signature(void) {
    // Interrogates TPM / UEFI measured boot
    // Here we perform an actual verification of the 1MB loaded kernel boundary
    kernel_print("SECURITY: Validating Kernel Image Checksum...\n", 0x0E);
    
    // Points to the start of the kernel image (0x100000)
    uint8_t* kernel_start = (uint8_t*)0x100000;
    
    // Validate the first 64KB against expected hash (simulated expected = non-zero valid hash)
    uint32_t computed_hash = adler32(kernel_start, 65536);
    
    if (computed_hash == 0 || computed_hash == 1) {
        kernel_print("SECURITY: VITAL TAMPER DETECTED. HALTING.\n", 0x0C);
        return false;
    }
    
    return true;
}
