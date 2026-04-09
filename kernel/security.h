// NeuronOS Capability-Based Security Model
// kernel/security.h

#ifndef SECURITY_H
#define SECURITY_H

#include <stdint.h>
#include <stdbool.h>

// Capability rights
#define CAP_READ      0x01
#define CAP_WRITE     0x02
#define CAP_EXECUTE   0x04
#define CAP_NETWORK   0x08
#define CAP_DEVICE    0x10
#define CAP_ADMIN     0x20

typedef struct {
    uint32_t object_id; // ID of the resource (file inode, process pid, etc.)
    uint32_t rights;    // Access rights granted
} capability_t;

// Access Control List (ACL) Entry
typedef struct acl_entry {
    uint32_t uid;
    uint32_t rights;
    struct acl_entry* next;
} acl_entry_t;

void security_init(void);

// Capability Grants and Checks
bool cap_grant(uint32_t pid, uint32_t object_id, uint32_t rights);
bool cap_revoke(uint32_t pid, uint32_t object_id);
bool cap_check(uint32_t pid, uint32_t object_id, uint32_t required_rights);

// Secure Boot verification stub
bool verify_kernel_signature(void);

#endif // SECURITY_H
