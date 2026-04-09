// NeuronOS Scheduler Header
// kernel/scheduler.h

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "kernel.h"
#include <stdbool.h>

// Scheduler API — definitions live in kernel.h
void    scheduler_init(void);
void    schedule(void);
void    scheduler_tick(void);
void    scheduler_boost_all(void);
void    scheduler_yield(void);
void    scheduler_block(void);
void    scheduler_unblock(process_t* proc);
void    scheduler_start(void);
void    scheduler_set_priority(process_t* proc, int priority);
void    scheduler_set_nice(process_t* proc, int nice);
void    scheduler_get_stats(scheduler_stats_t* stats);
void    scheduler_enable_ai(bool enable);

#endif // SCHEDULER_H
