// NeuronOS Loadable Kernel Modules
// kernel/module.h

#ifndef MODULE_H
#define MODULE_H

#include <stdint.h>
#include <stdbool.h>

#define MODULE_NAME_LEN 32

typedef struct {
    char name[MODULE_NAME_LEN];
    void* text_base;
    void* data_base;
    int (*init)(void);
    void (*cleanup)(void);
    bool is_loaded;
} kernel_module_t;

void module_init_system(void);
bool module_load(const char* filepath);
bool module_unload(const char* name);
kernel_module_t* get_module(const char* name);

#endif // MODULE_H
