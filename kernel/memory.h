// NeuronOS Memory Management Header
// kernel/memory.h
// Only memory-specific declarations that are NOT already in kernel.h

#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Page flags
#define PAGE_PRESENT    0x001
#define PAGE_WRITABLE   0x002
#define PAGE_USER       0x004
#define PAGE_ACCESSED   0x020
#define PAGE_DIRTY      0x040
#define PAGE_COW        0x200   // Copy-on-write (OS-available bit)

// Page size helpers
#define PAGE_SIZE       4096
#define PAGE_ALIGN(x)   (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

// snprintf — implemented in klibc.c
// (forward-declared here so any file that includes memory.h can use it)
int snprintf(char* buf, uint32_t size, const char* fmt, ...);

// String/memory utilities — implemented in klibc.c
void*    memset(void* dest, int val, uint32_t count);
void*    memcpy(void* dest, const void* src, uint32_t count);
int      memcmp(const void* a, const void* b, uint32_t count);
uint32_t strlen(const char* s);
int      strcmp(const char* a, const char* b);
int      strncmp(const char* a, const char* b, uint32_t n);
char*    strcpy(char* dest, const char* src);
char*    strncpy(char* dest, const char* src, uint32_t n);

// NOTE: All page_directory_t / page_table_t / page_fault_handler /
//       map_page / kalloc_page / kmalloc etc. are declared in kernel.h.
//       Do NOT duplicate them here to avoid conflicting-type errors.

#endif // MEMORY_H
