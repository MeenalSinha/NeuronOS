// NeuronOS AI Engine Public API
// kernel/ai/ai_engine.h

#ifndef AI_ENGINE_H
#define AI_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include "../kernel.h"
#include "../smp.h"

// Public AI Engine API (mirrors declarations in kernel.h)
int   ai_engine_init(void);
void  ai_record_execution(uint32_t pid, uint64_t cpu_time, uint64_t memory_used, uint64_t io_ops);
uint64_t ai_predict_cpu_burst(uint32_t pid);
bool  ai_detect_memory_leak(uint32_t pid);
int   ai_suggest_priority(uint32_t pid);
uint64_t ai_predict_time_quantum(uint32_t pid);
void  ai_get_stats(ai_stats_t* stats);
void  ai_validate_prediction(uint32_t pid, uint64_t actual_burst);
void  ai_allocate_resources(uint32_t pid, resource_allocation_t* allocation);

// Asynchronous telemetry pipeline
void  ai_push_telemetry(uint32_t pid, uint32_t cpu_time, uint32_t memory_used, uint32_t io_ops);
int   sys_ai_collect_telemetry(void* buffer, int max_records);
int   sys_ai_update_model(const void* model_buffer, int num_nodes, int num_features);

// Explainability
void  log_ai_decision(uint8_t decision_type, uint32_t pid, int16_t value, const char* reason);
void  ai_explain_decision(uint32_t pid, char* buffer, int max_len);
void  ai_get_decision_log(void** log_ptr, int* count);

// Persistence
void  ai_save_models(void);
void  ai_load_models(void);
void  ai_save_decision_log(void);
void  ai_load_decision_log(void);

// Proactive AI
process_t* ai_recommend_next_process(void);
void  ai_predict_system_load(void);
void  ai_predict_and_prefetch(uint32_t pid);
void  ai_trigger_background_optimization(void);

#endif // AI_ENGINE_H
