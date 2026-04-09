// NeuronOS Kernel Headers
// kernel/kernel.h

#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Kernel subsystem registry for AI monitoring
typedef enum {
    SUBSYS_SCHEDULER = 0,
    SUBSYS_MEMORY,
    SUBSYS_AI,
    SUBSYS_FS,
    SUBSYS_IPC,
    SUBSYS_SECURITY,
    SUBSYS_HEALING,
    SUBSYS_COUNT
} kernel_subsystem_t;

typedef struct {
    kernel_subsystem_t id;
    const char* name;
    bool initialized;
    bool ai_monitored;
    uint64_t health_score;  // 0-100, AI-computed
    uint32_t error_count;
    uint32_t ai_interventions;
} subsystem_status_t;

extern subsystem_status_t kernel_subsystems[SUBSYS_COUNT];

// Process states
typedef enum {
    PROCESS_STATE_UNUSED = 0,
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED,
    PROCESS_STATE_WAITING,
    PROCESS_STATE_ZOMBIE
} process_state_t;

// Thread states
typedef enum {
    THREAD_STATE_UNUSED = 0,
    THREAD_STATE_READY,
    THREAD_STATE_RUNNING,
    THREAD_STATE_BLOCKED,
    THREAD_STATE_TERMINATED
} thread_state_t;

// CPU context for context switching
typedef struct {
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp, esp;
    uint32_t eip;
    uint32_t eflags;
    uint32_t cr3;  // Page directory
} cpu_context_t;

// Forward declarations
typedef struct process process_t;
typedef struct thread thread_t;
typedef struct page_directory page_directory_t;
typedef struct file_descriptor file_descriptor_t;

#define MAX_PROCESSES 256
#define MAX_THREADS 1024
#define MAX_FD 128

// Thread structure
struct thread {
    uint32_t tid;
    thread_state_t state;
    process_t* process;
    
    // CPU context
    cpu_context_t context;
    
    // Stacks
    uint32_t kernel_stack;
    uint32_t user_stack;
    
    // Thread-local storage
    void* tls_base;
    
    // Scheduling
    uint32_t time_slice;
    uint32_t cpu_time;
};

// Process structure
struct process {
    uint32_t pid;
    process_state_t state;
    uint32_t parent_pid;
    
    char name[32];
    
    // Memory management
    page_directory_t* page_directory;
    uint32_t heap_start;
    uint32_t heap_end;
    uint32_t memory_usage;
    
    // Threads
    thread_t* main_thread;
    uint32_t thread_count;
    
    // Scheduling
    int priority;
    int nice;
    uint64_t cpu_time;
    uint32_t context_switches;
    
    // Statistics
    uint64_t page_faults;
    uint64_t io_operations;
    
    // AI-driven behavior history (NEW)
    struct {
        uint64_t cpu_bursts[16];      // Last 16 CPU burst times
        uint64_t io_wait_times[16];   // Last 16 I/O wait periods
        uint32_t fault_count;         // Total page faults
        uint32_t syscall_count;       // Total syscalls
        uint8_t burst_index;          // Circular buffer index
        uint8_t behavior_class;       // 0=unknown, 1=CPU-bound, 2=I/O-bound, 3=mixed
        int16_t ai_priority_delta;    // AI's priority adjustment (-10 to +10)
    } behavior;
    
    // File descriptors
    file_descriptor_t* fds[MAX_FD];
    int fd_count;
    
    // Exit code
    int exit_code;
};

// Page directory and table structures
typedef struct page_table {
    uint32_t entries[1024];
} page_table_t;

struct page_directory {
    uint32_t entries[1024];
};

// Boot info from bootloader
struct boot_info {
    uint32_t memory_size;
    uint32_t memory_map_addr;
    uint32_t kernel_start;
    uint32_t kernel_end;
};

// Kernel functions
void kernel_print(const char* str, uint8_t color);
void kernel_clear_screen(void);
void kernel_log(const char* subsystem, const char* message, uint8_t level);
void kernel_panic(const char* message);

// Memory management
int memory_init(struct boot_info* info);
void* kalloc_page(void);
void* kalloc_pages(uint32_t count);
void kfree_page(void* page);
void kfree_pages(void* pages, uint32_t count);
void* kmalloc(uint32_t size);
void kfree(void* ptr);
void* memset(void* dest, int val, uint32_t count);
void* memcpy(void* dest, const void* src, uint32_t count);

void map_page(page_directory_t* dir, uint32_t virt, uint32_t phys, uint32_t flags);
void unmap_page(page_directory_t* dir, uint32_t virt);
void copy_page_directory_cow(page_directory_t* src, page_directory_t* dest);
void page_fault_handler(uint32_t error_code, uint32_t faulting_addr);
page_directory_t* get_current_page_directory(void);
void switch_page_directory(page_directory_t* dir);
void enable_paging(page_directory_t* dir);

extern page_directory_t* kernel_page_directory;

// Process management
void process_init(void);
process_t* process_create(const char* name, void (*entry_point)(void*), void* arg);
thread_t* thread_create(process_t* process, void (*entry_point)(void*), void* arg);
process_t* process_fork(void);
void process_exit(int status);
int process_wait(uint32_t pid);
void context_switch(thread_t* from, thread_t* to);
process_t* get_process(uint32_t pid);
void thread_exit(void);
void switch_context(cpu_context_t* old_ctx, cpu_context_t* new_ctx);

extern process_t* current_process;
extern thread_t* current_thread;

// Scheduler
typedef struct {
    uint64_t total_context_switches;
    uint64_t schedule_calls;
    uint32_t ai_optimized_switches;
    uint32_t queue_counts[4];
    uint32_t queue_quantums[4];
    uint32_t avg_context_switch_time;
} scheduler_stats_t;

void scheduler_init(void);
void schedule(void);
void scheduler_tick(void);
void scheduler_boost_all(void);
void scheduler_yield(void);
void scheduler_block(void);
void scheduler_unblock(process_t* proc);
void scheduler_start(void);
void scheduler_set_priority(process_t* proc, int priority);
void scheduler_set_nice(process_t* proc, int nice);
void scheduler_get_stats(scheduler_stats_t* stats);
void scheduler_enable_ai(bool enable);

// AI Engine
typedef struct {
    uint64_t predictions_made;
    uint64_t predictions_accurate;
    uint64_t anomalies_detected;
    uint32_t processes_tracked;
    uint32_t leak_suspects;
    uint32_t accuracy_percent;
} ai_stats_t;

typedef struct {
    uint32_t cpu_quota;      // Percentage
    uint64_t memory_limit;   // Bytes
    int io_priority;         // -1 to 1
} resource_allocation_t;

int ai_engine_init(void);
void ai_record_execution(uint32_t pid, uint64_t cpu_time, uint64_t memory_used, uint64_t io_ops);
uint64_t ai_predict_cpu_burst(uint32_t pid);
bool ai_detect_memory_leak(uint32_t pid);
int ai_suggest_priority(uint32_t pid);
uint64_t ai_predict_time_quantum(uint32_t pid);
void ai_get_stats(ai_stats_t* stats);
void ai_validate_prediction(uint32_t pid, uint64_t actual_burst);
void ai_allocate_resources(uint32_t pid, resource_allocation_t* allocation);

// Interrupts and CPU
void gdt_init(void);
void idt_init(void);
void interrupts_init(void);
void timer_init(uint32_t frequency);
void timer_register_callback(void (*callback)(void));

// Drivers
void keyboard_driver_init(void);
void display_driver_init(void);
void disk_driver_init(void);

// Filesystem
int fs_init(void);

// IPC
void ipc_init(void);

// Security
void security_init(void);

// Self-healing
void healing_init(void);

// Tracing/Observability
void tracing_init(void);

// System calls
void syscall_init(void);
int syscall_wait(int pid);
void syscall_sleep(uint32_t ms);

// Init and daemons
int init_main(void* arg);
int shell_main(void* arg);
int ai_daemon_main(void* arg);
int healing_daemon_main(void* arg);
int logging_daemon_main(void* arg);

// Memory statistics
typedef struct {
    uint32_t total_frames;
    uint32_t used_frames;
    uint32_t free_frames;
    uint64_t page_faults;
    uint64_t cow_faults;
    uint32_t peak_usage;
} memory_stats_t;

void get_memory_stats(memory_stats_t* stats);

// ── Global tick counter (defined in main.c, incremented by PIT ISR) ──────────
extern volatile uint32_t kernel_tick_count;

// ── Timer API (implemented in kernel/timer.c or main.c) ──────────────────────
void timer_init(uint32_t frequency_hz);
void timer_register_callback(void (*callback)(void));
uint32_t timer_get_ticks(void);

// ── Scheduler tick (called every timer interrupt) ─────────────────────────────
void scheduler_tick(void);

#endif // KERNEL_H
