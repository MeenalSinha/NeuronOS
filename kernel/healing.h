// NeuronOS Self-Healing Subsystem
// kernel/healing.h

#ifndef HEALING_H
#define HEALING_H

#include <stdint.h>
#include <stdbool.h>

void healing_init(void);

// Watchdog timer ping
void healing_ping_watchdog(uint32_t process_id);

// Record panic state for next boot analysis
void healing_record_panic(const char* panic_reason, uint32_t instruction_pointer);

// Re-start a failed driver module
bool healing_restart_driver(const char* module_name);

// Rollback filesystem snapshot
bool healing_rollback_fs(uint32_t snapshot_id);

#endif // HEALING_H
