// NeuronOS System Call Dispatcher
// kernel/syscall.h

#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

// Initialize syscalls
void init_syscalls(void);

// The syscall handler that our assembly stub will call
uint32_t syscall_handler(uint32_t syscall_num, uint32_t arg1, uint32_t arg2, uint32_t arg3);

#endif // SYSCALL_H
