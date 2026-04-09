// NeuronOS Loadable Kernel Modules
// kernel/module.c

#include "module.h"
#include "kernel.h"
#include "memory.h"
#include "../fs/fs.h"

#define MAX_MODULES 32
static kernel_module_t modules[MAX_MODULES];

void module_init_system(void) {
    kernel_print("Initializing Kernel Module Subsystem...\n", 0x0F);
    for (int i = 0; i < MAX_MODULES; i++) {
        modules[i].is_loaded = false;
    }
}

// In a full implementation, this parses ELF headers, allocates kernel memory,
// relocates symbols, and jumps to the module's init function.
bool module_load(const char* filepath) {
    if (!filepath) return false;
    
    kernel_print("Loading module: ", 0x0F);
    kernel_print(filepath, 0x0F);
    kernel_print("\n", 0x0F);
    
    int free_slot = -1;
    for (int i = 0; i < MAX_MODULES; i++) {
        if (!modules[i].is_loaded) {
            free_slot = i;
            break;
        }
    }
    
    if (free_slot == -1) return false;
    
    // Simulate ELF loading
    kernel_module_t* mod = &modules[free_slot];
    int name_idx = 0;
    while (*filepath && name_idx < MODULE_NAME_LEN - 1) {
        mod->name[name_idx++] = *filepath++;
    }
    mod->name[name_idx] = '\0';
    
    mod->text_base = kmalloc(4096); // alloc 1 page for simulated text
    mod->data_base = kmalloc(4096); // alloc 1 page for simulated data
    mod->is_loaded = true;
    
    kernel_print("Module successfully loaded.\n", 0x0A);
    return true;
}

bool module_unload(const char* name) {
    for (int i = 0; i < MAX_MODULES; i++) {
        if (modules[i].is_loaded) {
            bool match = true;
            for(int j=0; j<MODULE_NAME_LEN; j++) {
                if (modules[i].name[j] != name[j]) { match = false; break; }
                if (name[j] == '\0') break;
            }
            if (match) {
                if(modules[i].text_base) kfree(modules[i].text_base);
                if(modules[i].data_base) kfree(modules[i].data_base);
                modules[i].is_loaded = false;
                kernel_print("Module unloaded.\n", 0x0E);
                return true;
            }
        }
    }
    return false;
}
