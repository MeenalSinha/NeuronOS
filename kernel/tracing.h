// NeuronOS Built-in Observability & Tracing
// kernel/tracing.h

#ifndef TRACING_H
#define TRACING_H

#include <stdint.h>

void tracing_init(void);

// Log an event trace (eBPF-like lightweight tracing)
void trace_event(const char* subsystem, const char* event_msg, uint32_t trace_data);

// Dump performance metrics dashboard
void tracing_dump_dashboard(void);

// eBPF-lite Virtual Machine types
typedef struct {
    uint8_t opcode;
    uint8_t dst;
    uint8_t src;
    int16_t off;
    int32_t imm;
} bpf_insn_t;

// Execute BPF program
uint32_t bpf_run(bpf_insn_t* prog, int len, uint32_t* mem);
void bpf_load_telemetry_prog(bpf_insn_t* prog, int len);
uint32_t tracing_run_bpf_telemetry(uint32_t pid, uint32_t cpu_time, uint32_t memory_used);

#endif // TRACING_H
