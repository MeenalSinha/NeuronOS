// NeuronOS SMP and Spinlock Implementations
// kernel/smp.c

#include "smp.h"
#include "kernel.h"

// Define Base Addresses for LAPIC
#define LAPIC_BASE 0xFEE00000
#define LAPIC_ID_REG 0x0020
#define LAPIC_ICR_LOW_REG 0x0300
#define LAPIC_ICR_HIGH_REG 0x0310

static volatile uint32_t* lapic = (volatile uint32_t*)LAPIC_BASE;

void smp_init(void) {
    kernel_print("Initializing Symmetric Multiprocessing (SMP)...\n", 0x0A);
    
    // In a real OS, we would parse the ACPI MADT tables here,
    // Wake up Application Processors (APs) using INIT-SIPI-SIPI sequence.
    kernel_print("LAPIC Virtual Mapping Engaged.\n", 0x0A);
}

// Atomically exchange values
static inline uint32_t atomic_xchg(volatile uint32_t *addr, uint32_t newval) {
    uint32_t result;
    asm volatile("lock xchgl %0, %1" :
                 "+m" (*addr), "=a" (result) :
                 "1" (newval) :
                 "cc");
    return result;
}

void spinlock_init(spinlock_t *lock) {
    lock->lock = 0;
    lock->cpu_id = 0xFF; // Unowned
}

void spinlock_acquire(spinlock_t *lock) {
    // Disable interrupts to prevent deadlocks on local core
    asm volatile("cli");
    
    uint32_t my_cpu = smp_get_current_cpu_id();
    
    while (1) {
        // Test and set
        if (!atomic_xchg(&lock->lock, 1)) {
            lock->cpu_id = my_cpu;
            break;
        }
        
        // Spin yielding CPU pipeline
        while (lock->lock) {
            asm volatile("pause");
        }
    }
}

void spinlock_release(spinlock_t *lock) {
    lock->cpu_id = 0xFF;
    
    // Memory barrier to ensure all writes complete before lock is released
    asm volatile("" ::: "memory");
    
    lock->lock = 0;
    
    // Re-enable interrupts
    asm volatile("sti");
}

uint32_t smp_get_current_cpu_id(void) {
    if (!lapic) return 0;
    return lapic[LAPIC_ID_REG / 4] >> 24;
}

void smp_send_ipi(uint8_t target_apic_id, uint8_t vector) {
    lapic[LAPIC_ICR_HIGH_REG / 4] = (target_apic_id << 24);
    lapic[LAPIC_ICR_LOW_REG / 4] = vector | 0x4000; // Fixed IPI
}
