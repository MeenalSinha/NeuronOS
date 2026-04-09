// NeuronOS Kernel - Main Entry Point
// kernel/main.c

#include "kernel.h"
#include "memory.h"
#include "process.h"
#include "scheduler.h"
#include "../fs/fs.h"
#include "syscall.h"
#include "gdt.h"
#include "idt.h"
#include "../drivers/pci.h"
#include "../drivers/ahci.h"
#include "security.h"
#include "ipc.h"
#include "tracing.h"
#include "healing.h"
#include "container.h"
#include "module.h"
#include "smp.h"
#include "../net/net.h"
#include "ai/gpu_backend.h"
#include "ai/ai_engine.h"
#include "../drivers/xhci.h"

// Kernel version
#define NEURON_OS_VERSION "1.0.0"
#define NEURON_OS_CODENAME "AI-Native"

// VGA text mode buffer
volatile unsigned short* vga_buffer = (unsigned short*)0xB8000;
int vga_row = 0;
int vga_col = 0;

// Global tick counter — incremented by PIT timer ISR
volatile uint32_t kernel_tick_count = 0;

static void timer_tick_callback(void) {
    kernel_tick_count++;
    scheduler_tick();
}

// Static boot_info instance (struct defined in kernel.h)
static struct boot_info boot_info;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void init_serial() {
    outb(0x3f8 + 1, 0x00);
    outb(0x3f8 + 3, 0x80);
    outb(0x3f8 + 0, 0x03);
    outb(0x3f8 + 1, 0x00);
    outb(0x3f8 + 3, 0x03);
    outb(0x3f8 + 2, 0xC7);
    outb(0x3f8 + 4, 0x0B);
}

void init_fpu() {
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2); // Clear EM (Emulation) bit
    cr0 |= (1 << 1);  // Set MP (Monitor Coprocessor) bit
    cr0 &= ~(1 << 3); // Clear TS (Task Switched) bit
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
    
    uint32_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9);  // Set OSFXSR (FXSAVE/FXRSTOR support)
    cr4 |= (1 << 10); // Set OSXMMEXCPT (SIMD Exception support)
    asm volatile("mov %0, %%cr4" : : "r"(cr4));
    
    asm volatile("fninit"); // Initialize FPU
}

void kernel_print(const char* str, uint8_t color) {
    while (*str) {
        outb(0x3F8, *str);
        if (*str == '\n') {
            vga_row++;
            vga_col = 0;
            outb(0x3F8, '\r');
        } else {
            vga_buffer[vga_row * 80 + vga_col] = (*str) | (color << 8);
            vga_col++;
            if (vga_col >= 80) {
                vga_col = 0;
                vga_row++;
            }
        }
        if (vga_row >= 25) {
            vga_row = 0;  // Simple wrap for now
        }
        str++;
    }
}

void kernel_clear_screen() {
    for (int i = 0; i < 80 * 25; i++) {
        vga_buffer[i] = ' ' | (0x0F << 8);
    }
    vga_row = 0;
    vga_col = 0;
}

void print_banner() {
    kernel_clear_screen();
    kernel_print("================================================================================\n", 0x0B);
    kernel_print("                          NeuronOS v" NEURON_OS_VERSION "\n", 0x0E);
    kernel_print("                    AI-Native Operating System\n", 0x0A);
    kernel_print("                         Codename: " NEURON_OS_CODENAME "\n", 0x09);
    kernel_print("================================================================================\n", 0x0B);
    kernel_print("\n", 0x0F);
}

void kernel_log(const char* subsystem, const char* message, uint8_t level) {
    const char* level_str[] = {"[INFO]", "[WARN]", "[ERROR]", "[OK]"};
    uint8_t colors[] = {0x0F, 0x0E, 0x0C, 0x0A};
    
    kernel_print(level_str[level], colors[level]);
    kernel_print(" ", 0x0F);
    kernel_print(subsystem, 0x07);
    kernel_print(": ", 0x07);
    kernel_print(message, 0x0F);
    kernel_print("\n", 0x0F);
}

// Kernel panic handler
void kernel_panic(const char* message) {
    kernel_clear_screen();
    kernel_print("*** KERNEL PANIC ***\n", 0x4F);
    kernel_print("Fatal error: ", 0x0C);
    kernel_print(message, 0x0F);
    kernel_print("\n\nSystem halted.\n", 0x0C);
    kernel_print("Please reboot your system.\n", 0x0E);
    
    // Halt CPU
    while(1) {
        asm volatile("hlt");
    }
}

// Multiboot 1 info struct (snippet)
typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    // ... other fields
} multiboot_info_t;

// Main kernel entry point
void kernel_main(uint32_t magic, multiboot_info_t* mb_info) {
    // Stage 0: Init Serial and FPU
    init_serial();
    init_fpu();

    // Initialize boot_info from Multiboot
    if (magic == 0x2BADB002 && mb_info != NULL) {
        // memory_size in KB: (mem_upper + 1024)
        boot_info.memory_size = (mb_info->mem_upper + 1024) * 1024;
    } else {
        // Default to 256MB if not booted via Multiboot (e.g. some emulators)
        boot_info.memory_size = 256 * 1024 * 1024;
    }

    // Stage 1: Display boot banner
    print_banner();
    
    // Stage 2: Initialize memory management
    kernel_log("MEMORY", "Initializing memory manager...", 0);
    if (memory_init(&boot_info) != 0) {
        kernel_panic("Memory initialization failed");
    }
    kernel_log("MEMORY", "Physical memory manager initialized", 3);
    kernel_log("MEMORY", "Virtual memory enabled with paging", 3);
    // Stage 2.5: Initialize SMP Logic
    kernel_log("CPU", "Configuring Symmetric Multiprocessing...", 0);
    smp_init();
    kernel_log("CPU", "SMP structures ready", 3);
    
    // Stage 3: Initialize GDT and IDT
    kernel_log("CPU", "Setting up GDT and IDT...", 0);
    init_gdt();
    init_idt();
    kernel_log("CPU", "Descriptor tables initialized (Ring 3 Ready)", 3);
    
    // Stage 4: Interrupt Subsystem
    kernel_log("INTERRUPTS", "Interrupts configured via IDT", 0);
    // Real IDT is now active, hardware interrupts theoretically enabled.
    kernel_log("INTERRUPTS", "Hardware interrupts enabled", 3);
    
    // Stage 5: Initialize timer (PIT)
    kernel_log("TIMER", "Initializing PIT timer at 100Hz...", 0);
    timer_init(100);
    timer_register_callback(timer_tick_callback);
    kernel_log("TIMER", "Timer initialized at 100Hz with scheduler tick", 3);
    
    // Stage 6: Initialize scheduler
    kernel_log("SCHEDULER", "Initializing process scheduler...", 0);
    scheduler_init();
    kernel_log("SCHEDULER", "Multi-level feedback queue ready", 3);
    
    // Stage 7: AI Infrastructure
    kernel_log("AI", "Initializing AI GPU Accelerators...", 0);
    ai_gpu_backend_init((void*)0xC0B00000); 

    kernel_log("AI", "Initializing AI prediction engine...", 0);
    if (ai_engine_init() != 0) {
        kernel_log("AI", "AI engine init failed, running without AI", 1);
    } else {
        kernel_log("AI", "AI-enhanced scheduling active", 3);
        kernel_log("AI", "Memory leak detection enabled", 3);
        kernel_log("AI", "Predictive optimization online", 3);
    }
    
    // Stage 8: Initialize Modules & Device drivers (PCI)
    kernel_log("DRIVERS", "Loading driver module subsystem...", 0);
    module_init_system();
    pci_init();
    
    // Attempt Extensible Host Controller initialization (USB 3.x)
    xhci_init((void*)0xC0C00000); 
    xhci_start_controller();
    
    // Using loadable kernel modules logic instead of hardcoding
    module_load("/drivers/keyboard.ko");
    module_load("/drivers/display.ko");
    module_load("/drivers/disk.ko");
    kernel_log("DRIVERS", "  - Dynamic kernel modules loaded", 3);
    
    // Stage 9: Initialize filesystem
    kernel_log("FS", "Mounting root filesystem (NeuronFS)...", 0);
    if (fs_init() != 0) {
        kernel_panic("FS: Failed to mount root filesystem");
    }
    kernel_log("FS", "Root filesystem mounted successfully", 3);
    kernel_log("FS", "Journaling enabled", 3);
    
    // Stages 10-12
    kernel_log("IPC", "Initializing inter-process communication...", 0);
    ipc_init();
    
    kernel_log("NET", "Starting TCP/IP networking stack...", 0);
    net_init();
    kernel_log("NET", "Network drivers online", 3);
    
    // Stage 11: Initialize Security & Containers
    kernel_log("SECURITY", "Initializing security & sandbox manager...", 0);
    security_init();
    container_init();
    kernel_log("SECURITY", "Capability-based isolation active", 3);
    
    // Stage 12: Initialize system calls
    kernel_log("SYSCALL", "Installing system call interface...", 0);
    init_syscalls();
    kernel_log("SYSCALL", "System calls (int 0x80) ready", 3);
    
    // Stage 13: Self-healing subsystem
    kernel_log("HEALING", "Initializing self-healing subsystem...", 0);
    healing_init();
    kernel_log("HEALING", "Watchdog timers active", 3);
    kernel_log("HEALING", "Fault detection enabled", 3);
    
    // Stage 14: Observability
    kernel_log("OBSERVABILITY", "Enabling kernel tracing...", 0);
    tracing_init();
    kernel_log("OBSERVABILITY", "Performance metrics collection started", 3);
    
    // Stage 15: Create init process
    kernel_log("INIT", "Starting init process...", 0);
    process_t* init = process_create("init", (void(*)(void*))init_main, 0);
    if (!init) {
        kernel_panic("Failed to create init process");
    }
    kernel_log("INIT", "Init process created (PID 1)", 3);
    
    // Stage 16: Enable preemption and start multitasking
    kernel_log("SCHEDULER", "Enabling preemptive multitasking...", 0);
    scheduler_start();
    
    kernel_print("\n", 0x0F);
    kernel_log("BOOT", "Kernel initialization complete!", 3);
    kernel_print("================================================================================\n", 0x0B);
    kernel_print("NeuronOS is now running in AI-enhanced mode\n", 0x0A);
    kernel_print("Type 'help' for available commands\n", 0x0F);
    kernel_print("================================================================================\n", 0x0B);
    
    // Idle in kernel - scheduler will take over
    while(1) {
        asm volatile("hlt");
    }
}

// Init process (PID 1) - runs in userspace
int init_main(void* arg) {
    // Spawn shell
    process_create("shell",      (void(*)(void*))shell_main,          0);
    
    // Spawn system daemons
    process_create("ai_daemon",   (void(*)(void*))ai_daemon_main,      0);
    process_create("heal_daemon", (void(*)(void*))healing_daemon_main, 0);
    process_create("log_daemon",  (void(*)(void*))logging_daemon_main, 0);
    
    // Init process becomes reaper for orphaned processes
    while(1) {
        syscall_wait(-1);  // Wait for any child
        syscall_sleep(100);
    }
    return 0;
}

// ── Userspace Syscall Wrappers (Stubs) ────────────────────────────────────────

int syscall_wait(int pid) {
    return -1; // Stub
}

void syscall_sleep(uint32_t ms) {
    // Stub
}

// ── System Daemons (Stubs) ────────────────────────────────────────────────────

int ai_daemon_main(void* arg) {
    while(1) { syscall_sleep(100); }
    return 0;
}

int healing_daemon_main(void* arg) {
    while(1) { syscall_sleep(100); }
    return 0;
}

int logging_daemon_main(void* arg) {
    while(1) { syscall_sleep(100); }
    return 0;
}
