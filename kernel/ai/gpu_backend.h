// NeuronOS - GPU Hardware Acceleration Backend for AI Subsystem
// kernel/ai/gpu_backend.h

#ifndef GPU_BACKEND_H
#define GPU_BACKEND_H

#include <stdint.h>
#include <stdbool.h>

// Command Queue structure similar to NVMe or OpenCL physical memory rings
typedef struct __attribute__((packed)) {
    uint32_t opcode;           // e.g., 0x1 = MATRIX_MULTIPLY, 0x2 = COMPUTE_TREE_SPLIT
    uint64_t source_physical;  // Physical address for telemetry DMA read
    uint64_t dest_physical;    // Physical address for resulting model struct
    uint32_t size;             // Data dimension size
    uint32_t status;           // Completion flag (updated via PCI interrupt)
} gpu_command_t;

// Context state for the GPU interface
typedef struct {
    volatile uint64_t *mmio_base;
    gpu_command_t *command_ring;
    uint32_t head;
    uint32_t tail;
    bool initialized;
} gpu_context_t;

// Initialization
void ai_gpu_backend_init(void *pcie_base_address);

// Send telemetry data directly to GPU via DMA
bool ai_gpu_dispatch_training(uint64_t phys_telemetry_matrix, uint64_t phys_model_output, uint32_t rows, uint32_t cols);

// Wait for GPU interrupt / completion ring
bool ai_gpu_wait_completion(void);

#endif // GPU_BACKEND_H
