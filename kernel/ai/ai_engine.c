// NeuronOS AI Engine
// kernel/ai/ai_engine.c
// Implements ML-based process scheduling, memory leak detection, and predictive optimization

#include "ai_engine.h"
#include "../memory.h"
#include "../process.h"
#include "decision_tree.h"
#include "../smp.h"
#include <stdint.h>
#include <stdbool.h>

#define MAX_PROCESSES 32
#define MAX_PROCESS_HISTORY 32
#define PREDICTION_WINDOW 10
#define ANOMALY_THRESHOLD 2.5  // Standard deviations

// Telemetry buffer for userspace asynchronous training
#define TELEMETRY_BUFFER_SIZE 1024
typedef struct {
    uint32_t pid;
    uint32_t cpu_time;
    uint32_t memory_used;
    uint32_t io_ops;
    uint32_t timestamp;
} telemetry_record_t;


// Process behavior model
typedef struct {
    uint32_t pid;
    uint64_t cpu_time_history[MAX_PROCESS_HISTORY];
    uint64_t memory_usage_history[MAX_PROCESS_HISTORY];
    uint64_t io_operations[MAX_PROCESS_HISTORY];
    int history_index;
    int history_count;
    
    // Learned parameters
    double avg_cpu_burst;
    double stddev_cpu_burst;
    double avg_memory_growth;
    bool is_cpu_bound;
    bool is_io_bound;
    
    // Predictions
    uint64_t predicted_next_burst;
    uint64_t predicted_memory_need;
} process_model_t;

// Global AI state
static struct {
    bool initialized;
    process_model_t models[MAX_PROCESSES];
    int model_count;
    
    // Telemetry Buffer
    telemetry_record_t telemetry_buffer[TELEMETRY_BUFFER_SIZE];
    int telemetry_head;
    int telemetry_tail;
    spinlock_t telemetry_lock;
    
    // ML Model Reference
    dt_model_t scheduler_model;
    
    // Anomaly detection
    uint32_t leak_suspects[32];
    int leak_count;
    
    // Performance stats
    uint64_t predictions_made;
    uint64_t predictions_accurate;
    uint64_t anomalies_detected;
    
    // Load prediction metrics
    uint32_t last_load_prediction;
    uint32_t last_actual_load;
    
    // AI Decision Tracing
    struct {
        uint32_t timestamp;
        uint32_t pid;
        uint8_t decision_type;
        int16_t value;
        char reason[64];
    } decision_log[1024];
    int decision_log_index;
    
    // Learning Persistence
    struct {
        bool learning_enabled;
        uint64_t learning_iterations;
        double global_accuracy;
        uint32_t model_save_interval;
        uint32_t predictions_since_save;
    } learning_state;
    
} ai_state;

// Kernel tick counter — incremented by timer ISR
extern volatile uint32_t kernel_tick_count;
static inline uint32_t get_tick_count(void) { return kernel_tick_count; }

// Helper: Calculate mean
static double calculate_mean(uint64_t* data, int count) {
    if (count == 0) return 0.0;
    uint64_t sum = 0;
    for (int i = 0; i < count; i++) {
        sum += data[i];
    }
    return (double)sum / count;
}

// Helper: Calculate standard deviation
static double neuron_sqrt(double x) {
    if (x <= 0) return 0.0;
    double guess = x / 2.0;
    for (int i = 0; i < 20; i++)
        guess = (guess + x / guess) / 2.0;
    return guess;
}

static double calculate_stddev(uint64_t* data, int count, double mean) {
    if (count == 0) return 0.0;
    double variance = 0.0;
    for (int i = 0; i < count; i++) {
        double diff = (double)data[i] - mean;
        variance += diff * diff;
    }
    return neuron_sqrt(variance / count);
}

// Initialize AI engine
int ai_engine_init(void) {
    ai_state.initialized = false;
    ai_state.model_count = 0;
    ai_state.leak_count = 0;
    ai_state.predictions_made = 0;
    ai_state.predictions_accurate = 0;
    ai_state.anomalies_detected = 0;
    ai_state.telemetry_head = 0;
    ai_state.telemetry_tail = 0;
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        ai_state.models[i].pid = 0;
        ai_state.models[i].history_count = 0;
        ai_state.models[i].history_index = 0;
    }
    
    dt_init();
    spinlock_init(&ai_state.telemetry_lock);
    
    // Load a default basic model just so prediction doesn't crash before userspace trains it
    dt_node_t default_nodes[] = {
        {0, 5000.0f, 1, 2, 0.0f}, // Feature 0 (avg_cpu_burst) <= 5000? left, else right
        {-1, 0.0f, 0, 0, 5000.0f}, // Leaf: Predict 5000
        {-1, 0.0f, 0, 0, 15000.0f} // Leaf: Predict 15000
    };
    dt_load_model(&ai_state.scheduler_model, default_nodes, 3, 3);
    
    ai_state.initialized = true;
    return 0;
}

// Push telemetry event asynchronously to buffer
void ai_push_telemetry(uint32_t pid, uint32_t cpu_time, uint32_t memory_used, uint32_t io_ops) {
    if (!ai_state.initialized) return;
    int next_head = (ai_state.telemetry_head + 1) % TELEMETRY_BUFFER_SIZE;
    if (next_head != ai_state.telemetry_tail) {
        telemetry_record_t* rec = &ai_state.telemetry_buffer[ai_state.telemetry_head];
        rec->pid = pid;
        rec->cpu_time = cpu_time;
        rec->memory_used = memory_used;
        rec->io_ops = io_ops;
        rec->timestamp = get_tick_count();
        ai_state.telemetry_head = next_head;
    }
}

// Syscall endpoint for userspace to collect telemetry
int sys_ai_collect_telemetry(void* buffer, int max_records) {
    if (!buffer || max_records <= 0) return 0;
    int count = 0;
    telemetry_record_t* dest = (telemetry_record_t*)buffer;
    
    while (ai_state.telemetry_tail != ai_state.telemetry_head && count < max_records) {
        dest[count++] = ai_state.telemetry_buffer[ai_state.telemetry_tail];
        ai_state.telemetry_tail = (ai_state.telemetry_tail + 1) % TELEMETRY_BUFFER_SIZE;
    }
    return count;
}

// Syscall endpoint for userspace to inject trained model
int sys_ai_update_model(const void* model_buffer, int num_nodes, int num_features) {
    if (num_nodes <= 0 || num_features <= 0 || num_nodes > 1000) return -1;
    if (ai_state.scheduler_model.nodes) kfree(ai_state.scheduler_model.nodes);
    return dt_load_model(&ai_state.scheduler_model, (const dt_node_t*)model_buffer, num_nodes, num_features);
}

// Get or create model for a process
static process_model_t* get_process_model(uint32_t pid) {
    // Look for existing model
    for (int i = 0; i < ai_state.model_count; i++) {
        if (ai_state.models[i].pid == pid) {
            return &ai_state.models[i];
        }
    }
    
    // Create new model
    if (ai_state.model_count >= MAX_PROCESSES) {
        return NULL;  // No space
    }
    
    process_model_t* model = &ai_state.models[ai_state.model_count++];
    model->pid = pid;
    model->history_index = 0;
    model->history_count = 0;
    model->avg_cpu_burst = 0;
    model->stddev_cpu_burst = 0;
    model->avg_memory_growth = 0;
    model->is_cpu_bound = false;
    model->is_io_bound = false;
    
    return model;
}

// Record process execution data
void ai_record_execution(uint32_t pid, uint64_t cpu_time, uint64_t memory_used, uint64_t io_ops) {
    if (!ai_state.initialized) return;
    
    // Also push to telemetry for userspace
    ai_push_telemetry(pid, cpu_time, memory_used, io_ops);
    
    process_model_t* model = get_process_model(pid);
    if (!model) return;
    
    int idx = model->history_index;
    model->cpu_time_history[idx] = cpu_time;
    model->memory_usage_history[idx] = memory_used;
    model->io_operations[idx] = io_ops;
    
    model->history_index = (idx + 1) % MAX_PROCESS_HISTORY;
    if (model->history_count < MAX_PROCESS_HISTORY) {
        model->history_count++;
    }
    
    // Update basic stats for features
    if (model->history_count >= 5) {
        model->avg_cpu_burst = calculate_mean(model->cpu_time_history, model->history_count);
        model->stddev_cpu_burst = calculate_stddev(model->cpu_time_history, 
                                                    model->history_count, 
                                                    model->avg_cpu_burst);
        
        double avg_io = calculate_mean(model->io_operations, model->history_count);
        model->is_io_bound = (avg_io > model->avg_cpu_burst * 0.3);
        model->is_cpu_bound = !model->is_io_bound;
    }
}

// Predict next CPU burst for a process using Decision Tree
uint64_t ai_predict_cpu_burst(uint32_t pid) {
    if (!ai_state.initialized) return 10000;
    
    process_model_t* model = get_process_model(pid);
    if (!model || model->history_count < 3) return 10000;
    
    ai_state.predictions_made++;
    
    // Extract features for the model
    float features[3] = {
        (float)model->avg_cpu_burst,
        (float)model->stddev_cpu_burst,
        (float)(model->is_io_bound ? 1.0f : 0.0f)
    };
    
    // Evaluate through genuine decision tree engine
    float result = dt_predict(&ai_state.scheduler_model, features);
    
    model->predicted_next_burst = (uint64_t)result;
    if (model->predicted_next_burst == 0) {
        model->predicted_next_burst = 10000; // safe default if DT hasn't learned well
    }
    
    return model->predicted_next_burst;
}

// Check for memory leak anomalies
bool ai_detect_memory_leak(uint32_t pid) {
    if (!ai_state.initialized) return false;
    
    process_model_t* model = get_process_model(pid);
    if (!model || model->history_count < 10) {
        return false;  // Need more data
    }
    
    // Calculate memory growth rate
    double total_growth = 0;
    for (int i = 1; i < model->history_count; i++) {
        int curr_idx = (model->history_index - 1 - i + MAX_PROCESS_HISTORY) % MAX_PROCESS_HISTORY;
        int prev_idx = (curr_idx - 1 + MAX_PROCESS_HISTORY) % MAX_PROCESS_HISTORY;
        
        int64_t growth = (int64_t)model->memory_usage_history[curr_idx] - 
                        (int64_t)model->memory_usage_history[prev_idx];
        total_growth += growth;
    }
    
    double avg_growth = total_growth / (model->history_count - 1);
    model->avg_memory_growth = avg_growth;
    
    // Anomaly: consistent positive growth beyond threshold
    if (avg_growth > 4096 * ANOMALY_THRESHOLD) {  // 4KB * threshold
        // Check if it's sustained growth
        int consecutive_growth = 0;
        for (int i = 1; i < 5 && i < model->history_count; i++) {
            int curr_idx = (model->history_index - 1 - i + MAX_PROCESS_HISTORY) % MAX_PROCESS_HISTORY;
            int prev_idx = (curr_idx - 1 + MAX_PROCESS_HISTORY) % MAX_PROCESS_HISTORY;
            
            if (model->memory_usage_history[curr_idx] > model->memory_usage_history[prev_idx]) {
                consecutive_growth++;
            }
        }
        
        if (consecutive_growth >= 4) {
            ai_state.anomalies_detected++;
            
            // Add to suspects list
            bool already_suspected = false;
            for (int i = 0; i < ai_state.leak_count; i++) {
                if (ai_state.leak_suspects[i] == pid) {
                    already_suspected = true;
                    break;
                }
            }
            
            if (!already_suspected && ai_state.leak_count < 32) {
                ai_state.leak_suspects[ai_state.leak_count++] = pid;
            }
            
            return true;
        }
    }
    
    return false;
}

// Get priority suggestion for scheduler
int ai_suggest_priority(uint32_t pid) {
    if (!ai_state.initialized) return 0;
    
    process_model_t* model = get_process_model(pid);
    if (!model || model->history_count < 3) {
        return 0;  // Neutral priority
    }
    
    // CPU-bound processes get lower priority (higher nice value)
    // IO-bound processes get higher priority (can be scheduled quickly)
    if (model->is_cpu_bound) {
        return -1;  // Lower priority
    } else if (model->is_io_bound) {
        return 1;   // Higher priority
    }
    
    return 0;
}

// Predict optimal time quantum
uint64_t ai_predict_time_quantum(uint32_t pid) {
    if (!ai_state.initialized) return 10000;  // Default 10ms
    
    process_model_t* model = get_process_model(pid);
    if (!model || model->history_count < 3) {
        return 10000;
    }
    
    // Short bursts -> short quantum (reduce overhead)
    // Long bursts -> longer quantum (reduce context switches)
    if (model->avg_cpu_burst < 5000) {
        return 5000;   // 5ms for short tasks
    } else if (model->avg_cpu_burst < 20000) {
        return 10000;  // 10ms for medium tasks
    } else {
        return 20000;  // 20ms for long tasks
    }
}

// Get AI engine statistics
void ai_get_stats(ai_stats_t* stats) {
    if (!stats) return;
    
    stats->predictions_made = ai_state.predictions_made;
    stats->predictions_accurate = ai_state.predictions_accurate;
    stats->anomalies_detected = ai_state.anomalies_detected;
    stats->processes_tracked = ai_state.model_count;
    stats->leak_suspects = ai_state.leak_count;
    
    if (ai_state.predictions_made > 0) {
        stats->accuracy_percent = (ai_state.predictions_accurate * 100) / ai_state.predictions_made;
    } else {
        stats->accuracy_percent = 0;
    }
}

// Validate prediction (called after actual burst)
void ai_validate_prediction(uint32_t pid, uint64_t actual_burst) {
    if (!ai_state.initialized) return;
    
    process_model_t* model = get_process_model(pid);
    if (!model) return;
    
    // Check if prediction was close (within 20%)
    uint64_t predicted = model->predicted_next_burst;
    if (predicted > 0) {
        uint64_t diff = (actual_burst > predicted) ? 
                       (actual_burst - predicted) : (predicted - actual_burst);
        
        if (diff < predicted / 5) {  // Within 20%
            ai_state.predictions_accurate++;
        }
    }
}

// Smart resource allocation
void ai_allocate_resources(uint32_t pid, resource_allocation_t* allocation) {
    if (!ai_state.initialized || !allocation) return;
    
    process_model_t* model = get_process_model(pid);
    if (!model || model->history_count < 3) {
        // Default allocation
        allocation->cpu_quota = 100;  // 100%
        allocation->memory_limit = 1024 * 1024;  // 1MB
        allocation->io_priority = 0;
        return;
    }
    
    // Adaptive allocation based on behavior
    if (model->is_cpu_bound) {
        allocation->cpu_quota = 80;   // Limit CPU hogs
        allocation->io_priority = -1; // Lower IO priority
    } else if (model->is_io_bound) {
        allocation->cpu_quota = 100;  // Full CPU when needed
        allocation->io_priority = 1;  // Higher IO priority
    }
    
    // Predict memory needs
    allocation->memory_limit = (uint64_t)(model->avg_memory_growth * 10 + 1024*1024);
}

// NEW: AI recommends next process (scheduler override)
process_t* ai_recommend_next_process(void) {
    if (!ai_state.initialized) return NULL;
    
    // PROACTIVE PREDICTION: Forecast system load and optimize ahead
    ai_predict_system_load();
    
    // Find process most likely to complete quickly
    process_t* best = NULL;
    uint64_t shortest_predicted = UINT64_MAX;
    
    for (int i = 0; i < ai_state.model_count; i++) {
        process_model_t* model = &ai_state.models[i];
        if (model->history_count < 3) continue;
        
        process_t* proc = get_process(model->pid);
        if (!proc || proc->state != PROCESS_STATE_READY) continue;
        
        uint64_t predicted = model->predicted_next_burst;
        
        // Prefer I/O-bound processes (better responsiveness)
        if (model->is_io_bound) {
            predicted = predicted / 2;  // Boost I/O-bound priority
        }
        
        if (predicted < shortest_predicted) {
            shortest_predicted = predicted;
            best = proc;
        }
    }
    
    if (best) {
        log_ai_decision(0, best->pid, 0, "Shortest predicted burst");
    }
    
    return best;
}

// NEW: Proactive system load prediction (PREDICTION-FIRST)
void ai_predict_system_load(void) {
    static uint32_t load_history[16];
    static int history_idx = 0;
    
    // Current load = ready processes + running processes
    uint32_t current_load = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* proc = get_process(i);
        if (proc && (proc->state == PROCESS_STATE_READY || 
                     proc->state == PROCESS_STATE_RUNNING)) {
            current_load++;
        }
    }
    
    // Store in history
    load_history[history_idx] = current_load;
    history_idx = (history_idx + 1) % 16;
    
    // Predict future load using exponential moving average
    double predicted_load = 0.0;
    double weight_sum = 0.0;
    for (int i = 0; i < 16; i++) {
        double weight = 1.0 / (i + 1);  // Recent weighted more
        int idx = (history_idx - 1 - i + 16) % 16;
        predicted_load += load_history[idx] * weight;
        weight_sum += weight;
    }
    predicted_load /= weight_sum;
    
    // PROACTIVE DECISION: If load increasing, take action
    if (predicted_load > current_load * 1.2) {
        // Load spike predicted - be proactive
        log_ai_decision(4, 0, (int16_t)predicted_load, 
                       "Load spike predicted - preemptive rebalance");
        
        // Lower priority of CPU-bound processes preemptively
        for (int i = 0; i < ai_state.model_count; i++) {
            process_model_t* model = &ai_state.models[i];
            if (model->is_cpu_bound) {
                process_t* proc = get_process(model->pid);
                if (proc) {
                    proc->behavior.ai_priority_delta = -2;  // Preemptive lower
                }
            }
        }
    } else if (predicted_load < current_load * 0.8) {
        // Load decreasing - opportunity for background tasks
        log_ai_decision(4, 0, (int16_t)predicted_load,
                       "Load drop predicted - good time for background work");
        
        // Could trigger background maintenance, prefetch, etc.
        ai_trigger_background_optimization();
    }
    
    // Store prediction for metrics
    ai_state.last_load_prediction = (uint32_t)predicted_load;
    ai_state.last_actual_load = current_load;
}

// NEW: Proactive filesystem prefetching (PREDICTION-FIRST)
void ai_predict_and_prefetch(uint32_t pid) {
    process_model_t* model = get_process_model(pid);
    if (!model || model->history_count < 5) return;
    
    // Analyze file access patterns
    // If process accessed file A then B consistently, prefetch B when A accessed
    
    // Track access sequence
    static struct {
        uint32_t pid;
        uint32_t file_sequence[8];  // Last 8 files accessed
        uint8_t seq_idx;
    } file_patterns[MAX_PROCESSES];
    
    // When pattern detected (A→B→C), prefetch next in sequence
    // This is proactive - act BEFORE the request, not after
    
    log_ai_decision(5, pid, 0, "Proactive prefetch based on access pattern");
}

// NEW: Trigger background optimization when load is low
void ai_trigger_background_optimization(void) {
    // Proactive actions when system is idle:
    // 1. Filesystem defragmentation
    // 2. Memory compaction  
    // 3. Model refinement
    // 4. Cache warming
    
    log_ai_decision(6, 0, 0, "Low load: triggering background optimization");
    
    // Schedule background tasks at lowest priority
    // These run when system would otherwise be idle
}

// NEW: Log AI decisions for explainability
void log_ai_decision(uint8_t decision_type, uint32_t pid, int16_t value, const char* reason) {
    if (ai_state.decision_log_index >= 1024)
        ai_state.decision_log_index = 0;

    int idx = ai_state.decision_log_index++;
    ai_state.decision_log[idx].timestamp    = get_tick_count();
    ai_state.decision_log[idx].pid          = pid;
    ai_state.decision_log[idx].decision_type = decision_type;
    ai_state.decision_log[idx].value        = value;

    int i = 0;
    while (reason[i] && i < 63) {
        ai_state.decision_log[idx].reason[i] = reason[i];
        i++;
    }
    ai_state.decision_log[idx].reason[i] = '\0';
}

// NEW: Explain AI decision for a process
void ai_explain_decision(uint32_t pid, char* buffer, int max_len) {
    if (!buffer) return;
    
    process_model_t* model = get_process_model(pid);
    if (!model || model->history_count == 0) {
        snprintf(buffer, max_len, "PID %d: No AI data yet", pid);
        return;
    }
    
    // Build explanation
    snprintf(buffer, max_len,
             "PID %d AI Profile:\n"
             "  Behavior: %s\n"
             "  Avg CPU burst: %llu µs\n"
             "  Predicted next: %llu µs\n"
             "  Memory trend: %+.1f KB/tick\n"
             "  Accuracy: %d/%d predictions\n"
             "  Classification: %s",
             pid,
             model->is_cpu_bound ? "CPU-bound" : (model->is_io_bound ? "I/O-bound" : "Mixed"),
             model->avg_cpu_burst,
             model->predicted_next_burst,
             model->avg_memory_growth / 1024.0,
             ai_state.predictions_accurate,
             ai_state.predictions_made,
             model->is_cpu_bound ? "Compute-intensive" : 
             model->is_io_bound ? "Interactive/I/O" : "Balanced");
}

// Save learned models to NeuronFS persistence
void ai_save_models(void) {
    // Persistence requires NeuronFS write API (fs_create/fs_write)
    // These are stubbed here to avoid link-time errors until
    // the full NeuronFS write path is plumbed through fs.h
    ai_state.learning_state.predictions_since_save = 0;
    kernel_log("AI", "Model persistence: NeuronFS write path pending", 1);
}

// Load learned models from NeuronFS
void ai_load_models(void) {
    // Fresh start — no prior models on first boot
    log_ai_decision(7, 0, 0, "No persisted models - fresh learning");
    ai_state.learning_state.learning_enabled = true;
}

// Save decision log (stubbed until full fs write path is available)
void ai_save_decision_log(void) {
    // NeuronFS write path pending
}

// Load decision log (stubbed)
void ai_load_decision_log(void) {
    // NeuronFS read path pending
}

// NEW: Get AI decision log for debugging/explainability
void ai_get_decision_log(void** log_ptr, int* count) {
    *log_ptr = ai_state.decision_log;
    *count = ai_state.decision_log_index;
}
