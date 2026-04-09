// NeuronOS PIT Timer Driver
// kernel/timer.c

#include "kernel.h"
#include "timer.h"

// I/O port access (inline asm helpers)
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// PIT channel 0 (IRQ0) constants
#define PIT_CHANNEL0   0x40
#define PIT_CMD        0x43
#define PIT_MODE3      0x36   // channel 0, lobyte/hibyte, square wave

static void (*tick_callback)(void) = NULL;

void timer_init(uint32_t frequency_hz) {
    if (frequency_hz == 0) frequency_hz = 100;

    // Divisor for PIT (1193180 / freq)
    uint32_t divisor = 1193180 / frequency_hz;
    if (divisor > 0xFFFF) divisor = 0xFFFF;

    // Set PIT mode 3 on channel 0
    outb(PIT_CMD, PIT_MODE3);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    kernel_log("TIMER", "PIT configured", 3);
}

void timer_register_callback(void (*callback)(void)) {
    tick_callback = callback;
}

// Called by IRQ0 handler (wired from arch_asm.asm isr_stub_handler)
void timer_handler(void) {
    if (tick_callback) {
        tick_callback();
    }
}

uint32_t timer_get_ticks(void) {
    return kernel_tick_count;
}

void timer_sleep(uint32_t milliseconds) {
    uint32_t start = kernel_tick_count;
    uint32_t ticks = milliseconds / 10; // assuming 100Hz = 10ms per tick
    while ((kernel_tick_count - start) < ticks) {
        __asm__ volatile ("hlt");
    }
}
