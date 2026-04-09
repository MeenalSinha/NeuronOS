// NeuronOS AI-Enhanced Multi-Level Feedback Queue Scheduler
// kernel/scheduler.c

#include "scheduler.h"
#include "kernel.h"
#include "process.h"
#include "ai/ai_engine.h"
#include "timer.h"
#include "tracing.h"
#include <stdint.h>
#include <stdbool.h>

// Forward declaration for AI schedule helper defined later in this file
static void ai_adjust_schedule(process_t* proc);

#define NUM_QUEUES 4
#define BASE_QUANTUM 10  // 10ms base time quantum

// Priority queues
typedef struct {
    process_t* queue[MAX_PROCESSES];
    int front, rear, count;
    uint32_t time_quantum;  // Time slice in milliseconds
} priority_queue_t;

static struct {
    priority_queue_t queues[NUM_QUEUES];
    bool enabled;
    bool preemption_enabled;
    uint64_t total_context_switches;
    uint64_t schedule_calls;
    
    // Current scheduling info
    process_t* current;
    uint32_t current_quantum_remaining;
    int current_queue_level;
    
    // AI integration
    bool ai_scheduling_enabled;
    uint32_t ai_optimized_switches;
    
} scheduler_state;

// Initialize scheduler
void scheduler_init(void) {
    // Initialize priority queues
    for (int i = 0; i < NUM_QUEUES; i++) {
        scheduler_state.queues[i].front = 0;
        scheduler_state.queues[i].rear = 0;
        scheduler_state.queues[i].count = 0;
        scheduler_state.queues[i].time_quantum = BASE_QUANTUM * (1 << i);  // Exponential
    }
    
    scheduler_state.enabled = false;
    scheduler_state.preemption_enabled = false;
    scheduler_state.total_context_switches = 0;
    scheduler_state.schedule_calls = 0;
    scheduler_state.current = NULL;
    scheduler_state.current_quantum_remaining = 0;
    scheduler_state.current_queue_level = 0;
    scheduler_state.ai_scheduling_enabled = true;
    scheduler_state.ai_optimized_switches = 0;
}

// Add process to appropriate queue
static void enqueue_mlfq(process_t* proc, int queue_level) {
    if (queue_level < 0) queue_level = 0;
    if (queue_level >= NUM_QUEUES) queue_level = NUM_QUEUES - 1;
    
    priority_queue_t* queue = &scheduler_state.queues[queue_level];
    
    if (queue->count >= MAX_PROCESSES) return;
    
    queue->queue[queue->rear] = proc;
    queue->rear = (queue->rear + 1) % MAX_PROCESSES;
    queue->count++;
}

// Remove process from queue
static process_t* dequeue_mlfq(int queue_level) {
    priority_queue_t* queue = &scheduler_state.queues[queue_level];
    
    if (queue->count == 0) return NULL;
    
    process_t* proc = queue->queue[queue->front];
    queue->front = (queue->front + 1) % MAX_PROCESSES;
    queue->count--;
    
    return proc;
}

// Find next process to run (MLFQ algorithm with AI override)
static process_t* select_next_process(void) {
    scheduler_state.schedule_calls++;
    
    // AI DECISION POINT: Let AI reorder/veto scheduling decisions
    if (scheduler_state.ai_scheduling_enabled) {
        process_t* ai_suggestion = ai_recommend_next_process();
        if (ai_suggestion && ai_suggestion->state == PROCESS_STATE_READY) {
            scheduler_state.ai_optimized_switches++;
            scheduler_state.current_quantum_remaining = 
                ai_predict_time_quantum(ai_suggestion->pid);
            scheduler_state.current = ai_suggestion;
            
            // Log AI decision for explainability
            kernel_log("SCHED", "AI override: selected next process", 0);
            
            return ai_suggestion;
        }
    }
    
    // Try each queue from highest to lowest priority
    for (int i = 0; i < NUM_QUEUES; i++) {
        if (scheduler_state.queues[i].count > 0) {
            process_t* proc = dequeue_mlfq(i);
            
            if (proc && proc->state == PROCESS_STATE_READY) {
                scheduler_state.current_queue_level = i;
                scheduler_state.current_quantum_remaining = 
                    scheduler_state.queues[i].time_quantum;
                
                // AI optimization: adjust quantum based on prediction
                if (scheduler_state.ai_scheduling_enabled) {
                    uint64_t predicted_burst = ai_predict_cpu_burst(proc->pid);
                    
                    // If prediction is available and reasonable
                    if (predicted_burst > 0 && predicted_burst < 100000) {
                        if (predicted_burst < scheduler_state.current_quantum_remaining * 1000) {
                            scheduler_state.current_quantum_remaining =
                                (predicted_burst / 1000) + 1;
                            kernel_log("SCHED", "AI quantum adjusted", 0);
                            scheduler_state.ai_optimized_switches++;
                        }
                    }
                    
                    // AI priority adjustment
                    ai_adjust_schedule(proc);
                }
                
                return proc;
            }
        }
    }
    
    return NULL;  // No ready processes
}

// Main scheduling function
void schedule(void) {
    if (!scheduler_state.enabled) return;
    
    process_t* prev = scheduler_state.current;
    process_t* next = select_next_process();
    
    if (!next) {
        // No process to run, idle
        return;
    }
    
    // Re-queue previous process if it's still ready
    if (prev && prev->state == PROCESS_STATE_READY && prev != next) {
        // Move down one queue level (MLFQ aging)
        int new_level = scheduler_state.current_queue_level + 1;
        if (new_level >= NUM_QUEUES) new_level = NUM_QUEUES - 1;
        enqueue_mlfq(prev, new_level);
    }
    
    if (next != prev) {
        scheduler_state.current = next;
        scheduler_state.total_context_switches++;
        
        if (prev && prev->main_thread && next->main_thread) {
            context_switch(prev->main_thread, next->main_thread);
        }
    }
}

// Timer interrupt handler for preemption
void scheduler_tick(void) {
    if (!scheduler_state.preemption_enabled) return;
    if (!scheduler_state.current) return;
    
    // Update CPU time
    scheduler_state.current->cpu_time++;
    
    // Decrement quantum
    if (scheduler_state.current_quantum_remaining > 0) {
        scheduler_state.current_quantum_remaining--;
    }
    
    // Time quantum expired?
    if (scheduler_state.current_quantum_remaining == 0) {
        // Trigger reschedule
        schedule();
    }
    
    // Periodic boost to prevent starvation (every 100 ticks)
    static uint32_t boost_counter = 0;
    boost_counter++;
    if (boost_counter >= 100) {
        boost_counter = 0;
        scheduler_boost_all();
    }
}

// Boost all processes to highest priority queue
void scheduler_boost_all(void) {
    // Collect all processes from all queues
    process_t* all_processes[MAX_PROCESSES];
    int count = 0;
    
    for (int i = 0; i < NUM_QUEUES; i++) {
        while (scheduler_state.queues[i].count > 0) {
            process_t* proc = dequeue_mlfq(i);
            if (proc) {
                all_processes[count++] = proc;
            }
        }
    }
    
    // Re-add all to highest priority queue
    for (int i = 0; i < count; i++) {
        enqueue_mlfq(all_processes[i], 0);
    }
}

// Yield CPU voluntarily
void scheduler_yield(void) {
    if (!scheduler_state.enabled) return;
    
    if (scheduler_state.current) {
        scheduler_state.current->state = PROCESS_STATE_READY;
    }
    
    schedule();
}

// Block current process
void scheduler_block(void) {
    if (!scheduler_state.current) return;
    
    scheduler_state.current->state = PROCESS_STATE_BLOCKED;
    schedule();
}

// Unblock a process
void scheduler_unblock(process_t* proc) {
    if (!proc) return;
    
    proc->state = PROCESS_STATE_READY;
    enqueue_mlfq(proc, 0);  // Add to highest priority
}

// Start the scheduler
void scheduler_start(void) {
    scheduler_state.enabled = true;
    scheduler_state.preemption_enabled = true;
    
    // Register timer callback
    timer_register_callback(scheduler_tick);
    
    // Initial schedule
    schedule();
}

// Set process priority
void scheduler_set_priority(process_t* proc, int priority) {
    if (!proc) return;
    
    proc->priority = priority;
    
    // Convert priority to queue level
    int queue_level = 2 - (priority / 10);  // Higher priority -> lower queue number
    if (queue_level < 0) queue_level = 0;
    if (queue_level >= NUM_QUEUES) queue_level = NUM_QUEUES - 1;
    
    // If process is ready, move to new queue
    if (proc->state == PROCESS_STATE_READY && proc != scheduler_state.current) {
        enqueue_mlfq(proc, queue_level);
    }
}

// Set nice value
void scheduler_set_nice(process_t* proc, int nice) {
    if (!proc) return;
    
    proc->nice = nice;
    scheduler_set_priority(proc, -nice);  // Nice is inverse of priority
}

// Get scheduler statistics
void scheduler_get_stats(scheduler_stats_t* stats) {
    if (!stats) return;
    
    stats->total_context_switches = scheduler_state.total_context_switches;
    stats->schedule_calls = scheduler_state.schedule_calls;
    stats->ai_optimized_switches = scheduler_state.ai_optimized_switches;
    
    for (int i = 0; i < NUM_QUEUES; i++) {
        stats->queue_counts[i] = scheduler_state.queues[i].count;
        stats->queue_quantums[i] = scheduler_state.queues[i].time_quantum;
    }
    
    if (scheduler_state.schedule_calls > 0) {
        stats->avg_context_switch_time = 
            scheduler_state.total_context_switches * 1000 / scheduler_state.schedule_calls;
    } else {
        stats->avg_context_switch_time = 0;
    }
}

// Round-robin scheduler (simpler alternative)
void schedule_round_robin(void) {
    static int current_index = 0;
    process_t* processes[MAX_PROCESSES];
    int count = 0;
    
    // Collect all ready processes
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* proc = get_process(i);
        if (proc && proc->state == PROCESS_STATE_READY) {
            processes[count++] = proc;
        }
    }
    
    if (count == 0) return;
    
    // Select next
    current_index = (current_index + 1) % count;
    process_t* next = processes[current_index];
    
    if (next != scheduler_state.current) {
        process_t* prev = scheduler_state.current;
        scheduler_state.current = next;
        
        if (prev && prev->main_thread && next->main_thread) {
            context_switch(prev->main_thread, next->main_thread);
        }
    }
}

// Priority-based scheduler
void schedule_priority(void) {
    process_t* highest_priority = NULL;
    int highest = -999999;
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* proc = get_process(i);
        if (proc && proc->state == PROCESS_STATE_READY) {
            if (proc->priority > highest) {
                highest = proc->priority;
                highest_priority = proc;
            }
        }
    }
    
    if (highest_priority && highest_priority != scheduler_state.current) {
        process_t* prev = scheduler_state.current;
        scheduler_state.current = highest_priority;
        
        if (prev && prev->main_thread && highest_priority->main_thread) {
            context_switch(prev->main_thread, highest_priority->main_thread);
        }
    }
}

// Enable/disable AI scheduling
void scheduler_enable_ai(bool enable) {
    scheduler_state.ai_scheduling_enabled = enable;
}

// AI-driven schedule adjustment (via eBPF telemetry & NN)
static void ai_adjust_schedule(process_t* proc) {
    if (!proc) return;
    
    // Evaluate the process using the eBPF Virtual Machine
    uint32_t bpf_weight = tracing_run_bpf_telemetry(proc->pid, proc->cpu_time, proc->heap_start);
    int ai_delta = 0;
    
    // eBPF weight scales dynamic priority
    if (bpf_weight > 5000) ai_delta = -1; // Lower priority for heavy loads
    else if (bpf_weight < 1000) ai_delta = 1; // Higher priority for light/IO tasks
    
    // Fallback or NN prediction hook
    ai_delta += ai_suggest_priority(proc->pid);
    
    if (ai_delta != 0) {
        int new_queue = scheduler_state.current_queue_level - ai_delta;
        if (new_queue < 0) new_queue = 0;
        if (new_queue >= NUM_QUEUES) new_queue = NUM_QUEUES - 1;
        
        if (new_queue != scheduler_state.current_queue_level) {
            kernel_log("SCHED", "BPF/AI priority delta applied", ai_delta);
            scheduler_state.current_queue_level = new_queue;
            proc->behavior.ai_priority_delta = (int16_t)ai_delta;
        }
    }
    
    // Starvation check: if process hasn't run in a long time, boost it
    if (proc->cpu_time == 0 && scheduler_state.schedule_calls > 1000) {
        kernel_log("SCHED", "AI starvation boost applied", 1);
        scheduler_state.current_queue_level = 0;
        proc->behavior.ai_priority_delta = 10;
    }
}

// Get AI decision history (for explainability — routed through ai_engine log)
void scheduler_get_ai_decisions(char** log_buffer, int* count) {
    void* log_ptr = NULL;
    ai_get_decision_log(&log_ptr, count);
    *log_buffer = (char*)log_ptr;
}
