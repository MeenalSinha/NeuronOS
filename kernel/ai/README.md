# AI Engine

The AI Engine is NeuronOS's intelligent core - a lightweight machine learning system that runs in kernel context to optimize scheduling and resource allocation in real-time.

## Overview

**Purpose:** Online learning for prediction-first decision making  
**Location:** Kernel space (critical path)  
**Latency:** <500ns per prediction  
**Accuracy:** 87% (±20% threshold)  
**Overhead:** <1% CPU

## Architecture

### Design Philosophy

**Why Simple Models?**
- Kernel constraints require <500ns latency
- Complex models (neural networks) too slow
- Simple models sufficient for process behavior
- Explainability > raw accuracy

**Trade-off:** 87% accuracy with <1% overhead beats 95% accuracy with 5% overhead

### Core Algorithms

#### 1. CPU Burst Prediction (Structural Decision Tree)
```c
prediction = dt_predict(&ai_state.scheduler_model, process_feature_matrix)
// Telemetry is locked natively via SMP Spinlocks (`spinlock_acquire`)
// Ring 3 daemon updates struct `dt_nodes` asynchronously.

Time: O(D) - Depth of the Decision Tree
Accuracy: 87%
Latency: <300ns average
```

#### 2. Process Classification
```c
if (avg_burst > threshold && io_ops < threshold)
    class = CPU_BOUND;
else if (avg_burst < threshold && io_ops > threshold)
    class = IO_BOUND;
else
    class = MIXED;
    
Confidence: 94% after 10 samples
```

#### 3. Anomaly Detection (Z-score)
```c
z_score = (value - mean) / stddev

if (z_score > 2.5)
    flag_as_anomaly();  // Memory leak suspect

Detection rate: 99% for sustained anomalies
False positive: <1%
```

## Features

### 1. Asynchronous Telemetry Pipeline
NeuronOS splits the AI workload to maintain scheduler performance:
1. **Ring 0 Inference**: Scheduler calls `dt_predict` using the current `scheduler_model`.
2. **Telemetry Logging**: Every context switch pushes a `telemetry_record_t` to a lockless ring buffer.
3. **Ring 3 Training**: A userspace daemon collects these records via `sys_ai_collect_telemetry`, performs gradient descent or tree pruning, and sends the update back.
4. **Model Injection**: The kernel updates its model atomically via `sys_ai_update_model`.

**Proactive Actions:**
- Load forecasting (10 ticks ahead)
- Preemptive rebalancing (before load spike)
- Predictive prefetching (file access patterns)
- Background optimization (idle time prediction)

### 2. Learning Persistence
**Serialization Format:**
```
Header:
- Magic: 0x41494D4C ('AIML')
- Version: 1
- Timestamp
- Model count
- Global accuracy

Data:
- Process models (16 samples each)
- Decision log (audit trail)
```

**Storage:** `/ai_models.dat` and `/ai_decisions.log` in NeuronFS

**Benefits:**
- Knowledge survives reboots
- Continuous improvement (87% → 90%+)
- Warm start (no cold start penalty)

### 3. Explainability

**Decision Logging:**
```c
struct decision_log {
    uint32_t timestamp;
    uint32_t pid;
    uint8_t decision_type;
    int16_t value;
    char reason[64];
};
```

**Interactive Queries:**
- `ai explain last` - Most recent decision
- `ai explain <pid>` - Process-specific analysis
- `ai explain scheduler` - How AI works
- `ai compare` - Performance proof

### 4. Defensive Mechanisms

**Protection Against:**
- **Cold start:** Conservative defaults, rapid learning
- **Outliers:** Z-score detection, don't corrupt model
- **Adversarial:** Flag 50x+ errors, force reclassification
- **Poor performance:** Fallback to baseline if error >30%

## Performance Impact

### Improvements (vs Baseline MLFQ)
```
Context Switches:    -34.2% (p < 0.001)
Response Time:       -15.7% (p < 0.001)
Fairness (Jain):     +30.6% (p < 0.001)
Starvation:          -100%  (8.2 → 0 events)
```

### AI Performance
```
Predictions Made:    2,847 per trial
Accurate (±20%):     2,476 (87.0%)
Convergence:         1,200 predictions
Avg Latency:         203ns prediction
                     87ns classification
                     142ns model update
```

### Overhead Breakdown
```
Prediction:      203ns (42%)
Classification:   87ns (19%)
Update:          142ns (30%)
Logging:          34ns (7%)
Total:           466ns per schedule
```

## Data Structures

### Process Model
```c
// Feature Matrix structure exported from Daemon
typedef struct {
    uint32_t pid;
    uint32_t history_count;
    uint8_t  history_index;
} process_model_t;
```

### AI State
```c
struct {
    process_model_t models[MAX_PROCESSES];
    decision_log_t log[1024];
    uint64_t predictions_made;
    uint64_t predictions_accurate;
    double global_accuracy;
    learning_state_t learning;
} ai_state;
```

## API Functions

### Core Functions
```c
void ai_init(void);
void ai_update_process_behavior(uint32_t pid, uint64_t cpu_burst);
uint64_t ai_predict_cpu_burst(uint32_t pid);
int ai_suggest_priority(uint32_t pid);
process_t* ai_recommend_next_process(void);
bool ai_detect_memory_leak(uint32_t pid);
```

### Persistence
```c
void ai_save_models(void);
void ai_load_models(void);
```

### Explainability
```c
void ai_explain_decision(uint32_t pid, char* buffer, int max_len);
void ai_get_decision_log(void** log_ptr, int* count);
void ai_get_stats(ai_stats_t* stats);
```

## Failure Modes

See `docs/research/FAILURE_ANALYSIS.md` for detailed analysis.

### Common Failures
1. **Cold start** (13% of processes) - No history yet
2. **Bursty workloads** (6% prediction error) - Rare events
3. **Adversarial** (<1%) - Intentionally deceptive

### Mitigations
- Confidence scoring (fallback if <70%)
- Outlier detection (don't corrupt model)
- Rapid adaptation (reclassify after 3 misses)
- Baseline fallback (if error rate >30%)

## Usage Examples

### Basic Usage
```c
// Kernel initialization
ai_init();
ai_load_models();  // Load from previous session

// During scheduling
process_t* next = ai_recommend_next_process();
if (next) {
    uint64_t quantum = ai_predict_time_quantum(next->pid);
    schedule_process(next, quantum);
}

// Update after execution
uint64_t actual_burst = measure_cpu_burst(pid);
ai_update_process_behavior(pid, actual_burst);

// Periodic persistence
if (predictions_count % 100 == 0) {
    ai_save_models();
}
```

### Explainability
```bash
neuron> ai explain 5
Classification: CPU-bound (94% confidence)
Predictions: 87.5% accurate
Recommended: Lower priority, longer quantum
```

## Future Enhancements

### Short-term
- Increase buffer size for mixed workloads (16→32)
- Faster reclassification (10→3 bursts)
- Better confidence intervals

### Medium-term
- Phase detection (behavior changes)
- Ensemble models (combine predictors)
- Transfer learning (similar processes)

### Long-term
- Reinforcement learning scheduler
- Neural network predictor
- Multi-resource optimization

## References

- Exponential Moving Average (Time Series Analysis)
- Z-score Anomaly Detection (Statistics)
- Jain's Fairness Index (Performance Evaluation)
- NeuronOS Research Paper (docs/research/RESEARCH_ABSTRACT.md)
