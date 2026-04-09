// NeuronOS SMP and Spinlock Implementations
// kernel/smp.h

#ifndef SMP_H
#define SMP_H

#include <stdint.h>
#include <stdbool.h>

// Spinlock structure
typedef struct {
    volatile uint32_t lock;
    uint32_t cpu_id;
} spinlock_t;

// SMP Initialization
void smp_init(void);

// Spinlock Operations
void spinlock_init(spinlock_t *lock);
void spinlock_acquire(spinlock_t *lock);
void spinlock_release(spinlock_t *lock);

// Inter-Processor Interrupts (IPIs)
void smp_send_ipi(uint8_t target_apic_id, uint8_t vector);
uint32_t smp_get_current_cpu_id(void);

#endif // SMP_H
