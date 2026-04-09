// NeuronOS Metrics Header
// kernel/metrics.h

#ifndef METRICS_H
#define METRICS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Comparison table used by metrics_export_comparison()
typedef struct {
    uint64_t context_switches_baseline;
    uint64_t context_switches_ai;
    double   context_switches_improvement;

    uint64_t response_time_baseline;
    uint64_t response_time_ai;
    double   response_time_improvement;

    double   cpu_util_baseline;
    double   cpu_util_ai;
    double   cpu_util_improvement;

    uint32_t fairness_baseline;
    uint32_t fairness_ai;
    double   fairness_improvement;

    uint32_t starvation_baseline;
    uint32_t starvation_ai;

    double   ai_overhead;
} comparison_table_t;

// Metrics API
void metrics_init(void);
void metrics_capture_baseline(void);
void metrics_record_context_switch(bool ai_optimized);
void metrics_record_response_time(uint64_t response_time_us, bool ai_scheduled);
void metrics_record_starvation(bool prevented_by_ai);
void metrics_record_ai_decision(bool override);
void metrics_record_ai_prediction(bool accurate);
void metrics_calculate_fairness(void);
void metrics_update_cpu_utilization(bool idle_tick);
void metrics_get_report(char* buffer, size_t max_len);
void metrics_export_comparison(comparison_table_t* table);
void metrics_record_proactive_action(uint8_t action_type);

#endif // METRICS_H
