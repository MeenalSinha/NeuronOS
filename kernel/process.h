// NeuronOS Process Management Header
// kernel/process.h

#ifndef PROCESS_H
#define PROCESS_H

#include "kernel.h"

// Process/thread API — definitions are in kernel.h
// This header exists so that files can include "process.h" cleanly.

void        process_init(void);
process_t*  process_create(const char* name, void (*entry_point)(void*), void* arg);
thread_t*   thread_create(process_t* process, void (*entry_point)(void*), void* arg);
process_t*  process_fork(void);
void        process_exit(int status);
int         process_wait(uint32_t pid);
void        context_switch(thread_t* from, thread_t* to);
process_t*  get_process(uint32_t pid);
void        thread_exit(void);
void        switch_context(cpu_context_t* old_ctx, cpu_context_t* new_ctx);

extern process_t*  current_process;
extern thread_t*   current_thread;

// Userspace daemon entry points
int init_main(void* arg);
int shell_main(void* arg);
int ai_daemon_main(void* arg);
int healing_daemon_main(void* arg);
int logging_daemon_main(void* arg);

#endif // PROCESS_H
