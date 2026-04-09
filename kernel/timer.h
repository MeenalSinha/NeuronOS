// NeuronOS Timer Header
// kernel/timer.h

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

// PIT (Programmable Interval Timer) frequencies
#define PIT_BASE_FREQUENCY 1193180   // Hz

// Timer API
void     timer_init(uint32_t frequency_hz);
void     timer_register_callback(void (*callback)(void));
uint32_t timer_get_ticks(void);
void     timer_sleep(uint32_t milliseconds);

// Internal ISR handler called by the IDT stub
void     timer_handler(void);

#endif // TIMER_H
