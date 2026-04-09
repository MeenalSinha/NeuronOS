// NeuronOS Built-in Observability & Tracing
// kernel/tracing.c
#include "tracing.h"
#include "kernel.h"
#include "memory.h"
#include "process.h"

#define TRACE_RING_BUFFER_SIZE 1024

// eBPF-lite Virtual Machine implementation
static uint32_t bpf_registers[10] = {0};

uint32_t bpf_run(bpf_insn_t* prog, int len, uint32_t* mem) {
    if (!prog || len == 0) return 0;
    
    for(int i=0; i<10; i++) bpf_registers[i] = 0;
    bpf_registers[1] = (uint32_t)mem; // R1 points to context/memory
    
    for (int pc = 0; pc < len && pc < 256; pc++) {
        bpf_insn_t ins = prog[pc];
        switch (ins.opcode) {
            case 0x07: // ADD_IMM
                bpf_registers[ins.dst] += ins.imm; break;
            case 0x0F: // ADD_REG
                bpf_registers[ins.dst] += bpf_registers[ins.src]; break;
            case 0x61: // LDX_MEM (load from context)
                bpf_registers[ins.dst] = ((uint32_t*)bpf_registers[ins.src])[ins.off]; break;
            case 0x95: // EXIT
                return bpf_registers[0]; // Return R0
            case 0x15: // JEQ_IMM
                if (bpf_registers[ins.dst] == (uint32_t)ins.imm) pc += ins.off; break;
            case 0x25: // JGT_IMM
                if (bpf_registers[ins.dst] > (uint32_t)ins.imm) pc += ins.off; break;
            case 0xB7: // MOV_IMM
                bpf_registers[ins.dst] = ins.imm; break;
            case 0xBF: // MOV_REG
                bpf_registers[ins.dst] = bpf_registers[ins.src]; break;
        }
    }
    return bpf_registers[0];
}

// User-injected BPF program for scheduling telemetry
static bpf_insn_t current_bpf_telemetry_prog[64];
static int current_bpf_prog_len = 0;

void bpf_load_telemetry_prog(bpf_insn_t* prog, int len) {
    if(len <= 0 || len > 64) return;
    for(int i=0; i<len; i++) current_bpf_telemetry_prog[i] = prog[i];
    current_bpf_prog_len = len;
}

uint32_t tracing_run_bpf_telemetry(uint32_t pid, uint32_t cpu_time, uint32_t memory_used) {
    if (current_bpf_prog_len == 0) {
        // Fallback default: just return memory used / 1024
        return memory_used / 1024;
    }
    uint32_t ctx[3] = {pid, cpu_time, memory_used};
    return bpf_run(current_bpf_telemetry_prog, current_bpf_prog_len, ctx);
}


typedef struct {
    uint32_t timestamp;
    char subsystem[16];
    char msg[64];
    uint32_t data;
} trace_record_t;

static trace_record_t ring_buffer[TRACE_RING_BUFFER_SIZE];
static int trace_head = 0;
static int trace_tail = 0;

void tracing_init(void) {
    kernel_print("Initializing Kernel Observability & Tracing...\n", 0x0F);
    trace_head = 0;
    trace_tail = 0;
}

void trace_event(const char* subsystem, const char* event_msg, uint32_t trace_data) {
    int next = (trace_head + 1) % TRACE_RING_BUFFER_SIZE;
    if (next == trace_tail) {
        // Drop oldest record by advancing tail
        trace_tail = (trace_tail + 1) % TRACE_RING_BUFFER_SIZE;
    }
    
    trace_record_t* rec = &ring_buffer[trace_head];
    rec->timestamp = kernel_tick_count;
    rec->data = trace_data;
    
    // Copy subsystem
    int i = 0;
    while(subsystem[i] && i < 15) { rec->subsystem[i] = subsystem[i]; i++; }
    rec->subsystem[i] = '\0';
    
    // Copy msg
    i = 0;
    while(event_msg[i] && i < 63) { rec->msg[i] = event_msg[i]; i++; }
    rec->msg[i] = '\0';
    
    trace_head = next;
}

void tracing_dump_dashboard(void) {
    kernel_print("\n=== SYSTEM OBSERVABILITY DASHBOARD ===\n", 0x0B);
    
    int curr = trace_tail;
    int count = 0;
    while (curr != trace_head && count < 10) {
        trace_record_t* rec = &ring_buffer[curr];
        kernel_print("[", 0x07);
        // Print timestamp
        kernel_print(rec->subsystem, 0x0E);
        kernel_print("] ", 0x07);
        kernel_print(rec->msg, 0x0F);
        kernel_print("\n", 0x0F);
        
        curr = (curr + 1) % TRACE_RING_BUFFER_SIZE;
        count++;
    }
}
