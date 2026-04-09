// NeuronOS Process and Thread Management
// kernel/process.c

#include "process.h"
#include "kernel.h"
#include "memory.h"
#include "scheduler.h"
#include "ai/ai_engine.h"
#include <stdint.h>
#include <stdbool.h>

#define MAX_PROCESSES 256
#define MAX_THREADS 1024
#define KERNEL_STACK_SIZE 8192
#define USER_STACK_SIZE 65536

// Process table
static process_t processes[MAX_PROCESSES];
static thread_t threads[MAX_THREADS];
static uint32_t next_pid = 1;
static uint32_t next_tid = 1;

// Current running process/thread
process_t* current_process = NULL;
thread_t* current_thread = NULL;

// Process queue
typedef struct process_queue {
    process_t* processes[MAX_PROCESSES];
    int front, rear, count;
} process_queue_t;

static process_queue_t ready_queue;
static process_queue_t waiting_queue;

// Forward declarations for queue helpers (defined later in this file)
static void         enqueue_process(process_queue_t* queue, process_t* proc);
static process_t*   dequeue_process(process_queue_t* queue, process_t* proc);

// Initialize process management
void process_init(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        processes[i].state = PROCESS_STATE_UNUSED;
        processes[i].pid = 0;
    }
    
    for (int i = 0; i < MAX_THREADS; i++) {
        threads[i].state = THREAD_STATE_UNUSED;
        threads[i].tid = 0;
    }
    
    ready_queue.front = ready_queue.rear = ready_queue.count = 0;
    waiting_queue.front = waiting_queue.rear = waiting_queue.count = 0;
}

// Allocate new process structure
static process_t* allocate_process(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROCESS_STATE_UNUSED) {
            return &processes[i];
        }
    }
    return NULL;
}

// Allocate new thread structure
static thread_t* allocate_thread(void) {
    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i].state == THREAD_STATE_UNUSED) {
            return &threads[i];
        }
    }
    return NULL;
}

// Create new process
process_t* process_create(const char* name, void (*entry_point)(void*), void* arg) {
    process_t* proc = allocate_process();
    if (!proc) return NULL;
    
    // Initialize process structure
    proc->pid = next_pid++;
    proc->state = PROCESS_STATE_READY;
    proc->parent_pid = current_process ? current_process->pid : 0;
    proc->priority = 0;
    proc->nice = 0;
    
    // Copy name
    int i;
    for (i = 0; i < 31 && name[i]; i++) {
        proc->name[i] = name[i];
    }
    proc->name[i] = '\0';
    
    // Allocate page directory (virtual memory)
    proc->page_directory = (page_directory_t*)kalloc_page();
    if (!proc->page_directory) {
        proc->state = PROCESS_STATE_UNUSED;
        return NULL;
    }
    
    // Initialize page directory
    memset(proc->page_directory, 0, 4096);
    
    // Map kernel space (higher half)
    for (uint32_t i = 768; i < 1024; i++) {
        proc->page_directory->entries[i] = kernel_page_directory->entries[i];
    }
    
    // Initialize statistics
    proc->cpu_time = 0;
    proc->context_switches = 0;
    proc->page_faults = 0;
    proc->memory_usage = 0;
    
    // File descriptors
    proc->fd_count = 0;
    for (int i = 0; i < MAX_FD; i++) {
        proc->fds[i] = NULL;
    }
    
    // Create main thread
    thread_t* thread = thread_create(proc, entry_point, arg);
    if (!thread) {
        kfree_page(proc->page_directory);
        proc->state = PROCESS_STATE_UNUSED;
        return NULL;
    }
    
    proc->main_thread = thread;
    proc->thread_count = 1;
    
    // Add to ready queue
    enqueue_process(&ready_queue, proc);
    
    return proc;
}

// Create new thread
thread_t* thread_create(process_t* process, void (*entry_point)(void*), void* arg) {
    thread_t* thread = allocate_thread();
    if (!thread) return NULL;
    
    thread->tid = next_tid++;
    thread->process = process;
    thread->state = THREAD_STATE_READY;
    
    // Allocate kernel stack
    thread->kernel_stack = (uint32_t)kalloc_pages(2);  // 8KB stack
    if (!thread->kernel_stack) {
        thread->state = THREAD_STATE_UNUSED;
        return NULL;
    }
    
    // Allocate user stack
    thread->user_stack = (uint32_t)kalloc_pages(16);  // 64KB stack
    if (!thread->user_stack) {
        kfree_pages((void*)thread->kernel_stack, 2);
        thread->state = THREAD_STATE_UNUSED;
        return NULL;
    }
    
    // Initialize context for first run
    thread->context.eip = (uint32_t)entry_point;
    thread->context.esp = thread->user_stack + USER_STACK_SIZE - 16;
    thread->context.ebp = thread->context.esp;
    thread->context.eflags = 0x202;  // IF enabled
    
    // Setup initial stack frame with argument
    uint32_t* stack = (uint32_t*)thread->context.esp;
    stack[0] = (uint32_t)arg;        // Argument
    stack[1] = (uint32_t)thread_exit; // Return address
    
    // TLS
    thread->tls_base = 0;
    
    return thread;
}

// Fork - create copy of current process
process_t* process_fork(void) {
    if (!current_process) return NULL;
    
    process_t* child = allocate_process();
    if (!child) return NULL;
    
    // Copy process structure
    *child = *current_process;
    child->pid = next_pid++;
    child->parent_pid = current_process->pid;
    child->state = PROCESS_STATE_READY;
    
    // Allocate new page directory
    child->page_directory = (page_directory_t*)kalloc_page();
    if (!child->page_directory) {
        child->state = PROCESS_STATE_UNUSED;
        return NULL;
    }
    
    // Copy page directory with COW (Copy-on-Write)
    copy_page_directory_cow(current_process->page_directory, child->page_directory);
    
    // Clone main thread
    thread_t* child_thread = allocate_thread();
    if (!child_thread) {
        kfree_page(child->page_directory);
        child->state = PROCESS_STATE_UNUSED;
        return NULL;
    }
    
    *child_thread = *current_thread;
    child_thread->tid = next_tid++;
    child_thread->process = child;
    
    // Allocate new stacks
    child_thread->kernel_stack = (uint32_t)kalloc_pages(2);
    child_thread->user_stack = (uint32_t)kalloc_pages(16);
    
    // Copy stack contents
    memcpy((void*)child_thread->kernel_stack, (void*)current_thread->kernel_stack, 8192);
    memcpy((void*)child_thread->user_stack, (void*)current_thread->user_stack, 65536);
    
    child->main_thread = child_thread;
    
    // Add to ready queue
    enqueue_process(&ready_queue, child);
    
    return child;
}

// Exit current process
void process_exit(int status) {
    if (!current_process) return;
    
    current_process->exit_code = status;
    current_process->state = PROCESS_STATE_ZOMBIE;
    
    // Wake up parent if waiting
    if (current_process->parent_pid) {
        process_t* parent = get_process(current_process->parent_pid);
        if (parent && parent->state == PROCESS_STATE_WAITING) {
            parent->state = PROCESS_STATE_READY;
            enqueue_process(&ready_queue, parent);
        }
    }
    
    // Orphan children to init
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].parent_pid == current_process->pid) {
            processes[i].parent_pid = 1;  // Init process
        }
    }
    
    // Schedule next process
    scheduler_yield();
}

// Wait for child process
int process_wait(uint32_t pid) {
    process_t* child = get_process(pid);
    if (!child || child->parent_pid != current_process->pid) {
        return -1;  // Not our child
    }
    
    if (child->state != PROCESS_STATE_ZOMBIE) {
        // Child still running, block
        current_process->state = PROCESS_STATE_WAITING;
        dequeue_process(&ready_queue, current_process);
        enqueue_process(&waiting_queue, current_process);
        scheduler_yield();
    }
    
    // Child is zombie, reap it
    int exit_code = child->exit_code;
    
    // Free resources
    kfree_page(child->page_directory);
    kfree_pages((void*)child->main_thread->kernel_stack, 2);
    kfree_pages((void*)child->main_thread->user_stack, 16);
    
    child->main_thread->state = THREAD_STATE_UNUSED;
    child->state = PROCESS_STATE_UNUSED;
    
    return exit_code;
}

// Context switch
void context_switch(thread_t* from, thread_t* to) {
    if (from == to) return;
    
    // Update statistics
    if (from && from->process) {
        from->process->context_switches++;
        
        // Record for AI engine
        ai_record_execution(from->process->pid, 
                          from->process->cpu_time,
                          from->process->memory_usage,
                          0);  // TODO: track IO
    }
    
    current_thread = to;
    current_process = to->process;
    
    // Switch page directory
    switch_page_directory(to->process->page_directory);
    
    // Switch context (assembly)
    switch_context(&from->context, &to->context);
}

// Get process by PID
process_t* get_process(uint32_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].pid == pid && processes[i].state != PROCESS_STATE_UNUSED) {
            return &processes[i];
        }
    }
    return NULL;
}

// Queue operations
static void enqueue_process(process_queue_t* queue, process_t* proc) {
    if (queue->count >= MAX_PROCESSES) return;
    
    queue->processes[queue->rear] = proc;
    queue->rear = (queue->rear + 1) % MAX_PROCESSES;
    queue->count++;
}

static process_t* dequeue_process(process_queue_t* queue, process_t* proc) {
    if (queue->count == 0) return NULL;
    
    // If specific process requested, find and remove it
    if (proc) {
        for (int i = 0; i < queue->count; i++) {
            int idx = (queue->front + i) % MAX_PROCESSES;
            if (queue->processes[idx] == proc) {
                // Shift remaining
                for (int j = i; j < queue->count - 1; j++) {
                    int curr = (queue->front + j) % MAX_PROCESSES;
                    int next = (queue->front + j + 1) % MAX_PROCESSES;
                    queue->processes[curr] = queue->processes[next];
                }
                queue->count--;
                queue->rear = (queue->rear - 1 + MAX_PROCESSES) % MAX_PROCESSES;
                return proc;
            }
        }
        return NULL;
    }
    
    // Remove from front
    process_t* p = queue->processes[queue->front];
    queue->front = (queue->front + 1) % MAX_PROCESSES;
    queue->count--;
    return p;
}

// Thread exit
void thread_exit(void) {
    if (!current_thread) return;
    
    current_thread->state = THREAD_STATE_TERMINATED;
    current_process->thread_count--;
    
    if (current_process->thread_count == 0) {
        process_exit(0);
    } else {
        scheduler_yield();
    }
}
