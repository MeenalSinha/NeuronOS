// NeuronOS System Call Dispatcher
// kernel/syscall.c

#include "syscall.h"
#include "kernel.h"
#include "ai_engine.h"
#include "../fs/neuronfs.h" // assuming this exists or fs functions are global

typedef uint32_t (*syscall_func_t)(uint32_t, uint32_t, uint32_t);

static syscall_func_t syscalls[256];

// Simple syscall functions forward mappings
static uint32_t sys_print(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    kernel_print((char*)arg1, (uint8_t)arg2);
    return 0;
}

static uint32_t sys_get_tick(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    return kernel_tick_count;
}

static uint32_t sys_ai_collect(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    return sys_ai_collect_telemetry((void*)arg1, (int)arg2);
}

static uint32_t sys_ai_update(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    return sys_ai_update_model((const void*)arg1, (int)arg2, (int)arg3);
}

void init_syscalls(void) {
    for(int i=0; i<256; i++) {
        syscalls[i] = 0;
    }
    
    // Register syscalls
    syscalls[1] = sys_print;
    syscalls[2] = sys_get_tick;
    syscalls[100] = sys_ai_collect;
    syscalls[101] = sys_ai_update;
}

uint32_t syscall_handler(uint32_t syscall_num, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    if (syscall_num >= 256 || !syscalls[syscall_num]) {
        return (uint32_t)-1; // Invalid syscall
    }
    
    return syscalls[syscall_num](arg1, arg2, arg3);
}
