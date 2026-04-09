// NeuronOS Memory Management
// kernel/memory.c
// Implements paging, virtual memory, COW fork, demand paging

#include "kernel.h"
#include "memory.h"
#include <stdint.h>
#include <stdbool.h>

#define PAGE_SIZE 4096
#define PAGES_PER_TABLE 1024
#define TABLES_PER_DIR 1024
#define KERNEL_HEAP_START 0xC0000000
#define KERNEL_HEAP_SIZE (16 * 1024 * 1024)  // 16MB

// Page frame allocator
#define MAX_FRAMES 262144  // Support up to 1GB RAM
static uint32_t frame_bitmap[MAX_FRAMES / 32];
static uint32_t total_frames = 0;
static uint32_t used_frames = 0;

// Kernel page directory
page_directory_t* kernel_page_directory = NULL;

// Slab / Bucket Allocator State
#define NUM_BUCKETS 8
static uint32_t bucket_sizes[NUM_BUCKETS] = {32, 64, 128, 256, 512, 1024, 2048, 4096};
typedef struct free_block {
    struct free_block* next;
} free_block_t;
static free_block_t* buckets[NUM_BUCKETS] = {NULL};

typedef struct {
    uint32_t size;
    uint32_t magic;
} mem_header_t;
#define MEM_MAGIC 0xCAFEBABE

// Statistics
static struct {
    uint64_t page_faults;
    uint64_t cow_faults;
    uint64_t allocations;
    uint64_t deallocations;
    uint32_t peak_usage;
} memory_stats;

// Forward declarations for internal helpers
static void set_frame(uint32_t frame_num);
static void clear_frame(uint32_t frame_num);

// Initialize memory management
int memory_init(struct boot_info* info) {
    // Parse memory map from bootloader
    total_frames = info->memory_size / PAGE_SIZE;
    if (total_frames > MAX_FRAMES) {
        total_frames = MAX_FRAMES;
    }
    
    // Initialize frame bitmap (all free)
    for (uint32_t i = 0; i < MAX_FRAMES / 32; i++) {
        frame_bitmap[i] = 0;
    }
    
    
    // Create kernel page directory
    kernel_page_directory = (page_directory_t*)0x400000;  // At 4MB
    memset(kernel_page_directory, 0, sizeof(page_directory_t));
    // 1. Identity map ALL 256MB of RAM (64 tables)
    // 2. Map virtual Higher-Half (0xC0000000+) to physical 64MB-192MB (32 tables)
    // Use 0x401000 onwards for tables
    for (uint32_t t = 0; t < 64; t++) {
        page_table_t* table = (page_table_t*)(0x401000 + t * 0x1000);
        for (uint32_t i = 0; i < 1024; i++) {
            uint32_t phys = (t * 1024 + i) * PAGE_SIZE;
            table->entries[i] = phys | 0x3;
        }
        kernel_page_directory->entries[t] = (uint32_t)table | 0x3;
    }
    
    for (uint32_t t = 0; t < 32; t++) {
        page_table_t* table = (page_table_t*)(0x441000 + t * 0x1000);
        for (uint32_t i = 0; i < 1024; i++) {
            uint32_t phys = (64 * 1024 * 1024 / PAGE_SIZE + t * 1024 + i) * PAGE_SIZE;
            table->entries[i] = phys | 0x3;
        }
        kernel_page_directory->entries[768 + t] = (uint32_t)table | 0x3;
    }
    
    // Reserve first 192MB (Identity Core + Higher-Half Heap Physical Range)
    used_frames = 49152; 
    for (uint32_t i = 0; i < 49152; i++) set_frame(i);
    
    // Enable paging
    enable_paging(kernel_page_directory);
    
    // Initialize Slab Buckets
    for(int i=0; i<NUM_BUCKETS; i++) {
        buckets[i] = NULL;
    }
    
    memory_stats.page_faults = 0;
    memory_stats.cow_faults = 0;
    memory_stats.allocations = 0;
    memory_stats.deallocations = 0;
    memory_stats.peak_usage = 0;
    
    return 0;
}

// Frame allocation
static void set_frame(uint32_t frame_num) {
    uint32_t idx = frame_num / 32;
    uint32_t bit = frame_num % 32;
    frame_bitmap[idx] |= (1 << bit);
}

static void clear_frame(uint32_t frame_num) {
    uint32_t idx = frame_num / 32;
    uint32_t bit = frame_num % 32;
    frame_bitmap[idx] &= ~(1 << bit);
}

static bool test_frame(uint32_t frame_num) {
    uint32_t idx = frame_num / 32;
    uint32_t bit = frame_num % 32;
    return (frame_bitmap[idx] & (1 << bit)) != 0;
}

// Allocate physical frame
static uint32_t alloc_frame(void) {
    for (uint32_t i = 0; i < total_frames; i++) {
        if (!test_frame(i)) {
            set_frame(i);
            used_frames++;
            if (used_frames > memory_stats.peak_usage) {
                memory_stats.peak_usage = used_frames;
            }
            return i;
        }
    }
    return 0xFFFFFFFF;  // Out of memory
}

// Free physical frame
static void free_frame(uint32_t frame_num) {
    if (frame_num < total_frames && test_frame(frame_num)) {
        clear_frame(frame_num);
        used_frames--;
    }
}

// Allocate page (4KB)
void* kalloc_page(void) {
    uint32_t frame = alloc_frame();
    if (frame == 0xFFFFFFFF) return NULL;
    
    memory_stats.allocations++;
    return (void*)(frame * PAGE_SIZE);
}

// Allocate multiple pages
void* kalloc_pages(uint32_t count) {
    // Find contiguous frames
    uint32_t start_frame = 0xFFFFFFFF;
    uint32_t consecutive = 0;
    
    for (uint32_t i = 0; i < total_frames; i++) {
        if (!test_frame(i)) {
            if (consecutive == 0) {
                start_frame = i;
            }
            consecutive++;
            
            if (consecutive == count) {
                // Found enough frames
                for (uint32_t j = 0; j < count; j++) {
                    set_frame(start_frame + j);
                }
                used_frames += count;
                memory_stats.allocations++;
                return (void*)(start_frame * PAGE_SIZE);
            }
        } else {
            consecutive = 0;
        }
    }
    
    return NULL;  // Not enough contiguous memory
}

// Free page
void kfree_page(void* page) {
    uint32_t frame = (uint32_t)page / PAGE_SIZE;
    free_frame(frame);
    memory_stats.deallocations++;
}

// Free multiple pages
void kfree_pages(void* pages, uint32_t count) {
    uint32_t start_frame = (uint32_t)pages / PAGE_SIZE;
    for (uint32_t i = 0; i < count; i++) {
        free_frame(start_frame + i);
    }
    memory_stats.deallocations++;
}

// Heap allocation (Slab Bucket Allocator)
void* kmalloc(uint32_t size) {
    if (size == 0) return NULL;
    
    uint32_t total_size = size + sizeof(mem_header_t);
    int bucket = -1;
    for (int i = 0; i < NUM_BUCKETS; i++) {
        if (total_size <= bucket_sizes[i]) {
            bucket = i;
            break;
        }
    }
    
    if (bucket == -1) {
        // Large allocation (Multi-page)
        uint32_t pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;
        void* ptr = kalloc_pages(pages);
        if (!ptr) return NULL;
        mem_header_t* hdr = (mem_header_t*)ptr;
        hdr->size = pages * PAGE_SIZE;
        hdr->magic = MEM_MAGIC;
        memory_stats.allocations++;
        return (void*)((char*)ptr + sizeof(mem_header_t));
    }
    
    if (buckets[bucket] != NULL) {
        // Fast path O(1) allocation
        mem_header_t* hdr = (mem_header_t*)buckets[bucket];
        buckets[bucket] = (free_block_t*)(*((uint32_t*)hdr)); 
        
        hdr->size = bucket_sizes[bucket];
        hdr->magic = MEM_MAGIC;
        memory_stats.allocations++;
        return (void*)((char*)hdr + sizeof(mem_header_t));
    }
    
    // Bucket empty, allocate new page slab
    void* page = kalloc_page();
    if (!page) {
        kernel_log("MEMORY", "kmalloc failed: Out of physical memory", 1);
        return NULL;
    }
    
    uint32_t block_size = bucket_sizes[bucket];
    uint32_t num_blocks = PAGE_SIZE / block_size;
    
    char* ptr = (char*)page;
    for (uint32_t i = 1; i < num_blocks; i++) {
        uint32_t* next_ptr = (uint32_t*)(ptr + i * block_size);
        *next_ptr = (uint32_t)buckets[bucket];
        buckets[bucket] = (free_block_t*)next_ptr;
    }
    
    mem_header_t* hdr = (mem_header_t*)page;
    hdr->size = block_size;
    hdr->magic = MEM_MAGIC;
    memory_stats.allocations++;
    return (void*)((char*)hdr + sizeof(mem_header_t));
}

// Free heap allocation
void kfree(void* ptr) {
    if (!ptr) return;
    
    mem_header_t* hdr = (mem_header_t*)((char*)ptr - sizeof(mem_header_t));
    if (hdr->magic != MEM_MAGIC) {
        kernel_log("MEMORY", "kfree failed: Invalid magic boundary tag", 1);
        return;
    }
    
    uint32_t block_size = hdr->size;
    
    if (block_size >= PAGE_SIZE && (block_size % PAGE_SIZE == 0)) {
        kfree_pages((void*)hdr, block_size / PAGE_SIZE);
        memory_stats.deallocations++;
        return;
    }
    
    int bucket = -1;
    for (int i = 0; i < NUM_BUCKETS; i++) {
        if (block_size == bucket_sizes[i]) {
            bucket = i;
            break;
        }
    }
    
    if (bucket != -1) {
        // Return to O(1) free list
        uint32_t* next_ptr = (uint32_t*)hdr;
        *next_ptr = (uint32_t)buckets[bucket];
        buckets[bucket] = (free_block_t*)hdr;
        memory_stats.deallocations++;
    }
}

// Map virtual page to physical frame
void map_page(page_directory_t* dir, uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t dir_idx = virt >> 22;
    uint32_t table_idx = (virt >> 12) & 0x3FF;
    
    // Get or create page table
    page_table_t* table;
    if (!(dir->entries[dir_idx] & 0x1)) {
        // Table doesn't exist, create it
        table = (page_table_t*)kalloc_page();
        memset(table, 0, PAGE_SIZE);
        dir->entries[dir_idx] = ((uint32_t)table) | flags | 0x1;
    } else {
        table = (page_table_t*)(dir->entries[dir_idx] & ~0xFFF);
    }
    
    // Map page
    table->entries[table_idx] = phys | flags | 0x1;
}

// Unmap virtual page
void unmap_page(page_directory_t* dir, uint32_t virt) {
    uint32_t dir_idx = virt >> 22;
    uint32_t table_idx = (virt >> 12) & 0x3FF;
    
    if (!(dir->entries[dir_idx] & 0x1)) return;  // Table doesn't exist
    
    page_table_t* table = (page_table_t*)(dir->entries[dir_idx] & ~0xFFF);
    table->entries[table_idx] = 0;
    
    // Invalidate TLB
    asm volatile("invlpg (%0)" : : "r"(virt));
}

// Copy page directory with Copy-on-Write
void copy_page_directory_cow(page_directory_t* src, page_directory_t* dest) {
    for (uint32_t i = 0; i < 1024; i++) {
        if (src->entries[i] & 0x1) {
            if (i >= 768) {
                // Kernel space - share directly
                dest->entries[i] = src->entries[i];
            } else {
                // User space - setup COW
                page_table_t* src_table = (page_table_t*)(src->entries[i] & ~0xFFF);
                page_table_t* dest_table = (page_table_t*)kalloc_page();
                
                for (uint32_t j = 0; j < 1024; j++) {
                    if (src_table->entries[j] & 0x1) {
                        // Mark both as read-only for COW
                        src_table->entries[j] &= ~0x2;   // Clear RW bit
                        dest_table->entries[j] = src_table->entries[j];
                        
                        // Increment reference count (simplified - would use refcount array)
                    }
                }
                
                dest->entries[i] = ((uint32_t)dest_table) | 0x5;  // Present, User, RO
            }
        }
    }
}

// Page fault handler
void page_fault_handler(uint32_t error_code, uint32_t faulting_addr) {
    memory_stats.page_faults++;
    
    bool present = error_code & 0x1;
    bool write = error_code & 0x2;
    bool user = error_code & 0x4;
    
    // Check if this is a COW fault
    if (present && write) {
        // COW fault - copy page
        memory_stats.cow_faults++;
        
        uint32_t page_aligned = faulting_addr & ~0xFFF;
        
        // Allocate new frame
        uint32_t new_frame = alloc_frame();
        if (new_frame == 0xFFFFFFFF) {
            // Out of memory - kernel panic
            return;
        }
        
        // Prevent recursive fault by mapping new physical frame to a temporary virtual address
        uint32_t temp_virt = 0xC03FF000;
        map_page(get_current_page_directory(), temp_virt, new_frame * PAGE_SIZE, 0x3);
        asm volatile("invlpg (%0)" : : "r"(temp_virt));
        
        // Copy the data using the safe virtual addresses
        memcpy((void*)temp_virt, (void*)page_aligned, PAGE_SIZE);
        
        // Re-map the faulting virtual address to the new frame with write permission
        page_directory_t* dir = get_current_page_directory();
        uint32_t dir_idx = page_aligned >> 22;
        uint32_t table_idx = (page_aligned >> 12) & 0x3FF;
        
        page_table_t* table = (page_table_t*)(dir->entries[dir_idx] & ~0xFFF);
        table->entries[table_idx] = (new_frame * PAGE_SIZE) | 0x7;  // Present, RW, User
        
        // Invalidate TLBs and cleanup
        asm volatile("invlpg (%0)" : : "r"(page_aligned));
        unmap_page(get_current_page_directory(), temp_virt);
        
        return;
    }
    
    // Demand paging - allocate page on access
    if (!present) {
        uint32_t page_aligned = faulting_addr & ~0xFFF;
        uint32_t new_frame = alloc_frame();
        
        if (new_frame != 0xFFFFFFFF) {
            uint32_t flags = 0x7;  // Present, RW, User
            if (!user) flags &= ~0x4;  // Kernel page
            
            map_page(get_current_page_directory(), page_aligned, 
                    new_frame * PAGE_SIZE, flags);
            return;
        }
    }
    
    // Unhandled page fault - would trigger kernel panic
}

// Get current page directory
page_directory_t* get_current_page_directory(void) {
    uint32_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    return (page_directory_t*)cr3;
}

// Switch page directory
void switch_page_directory(page_directory_t* dir) {
    asm volatile("mov %0, %%cr3" : : "r"(dir));
}

// Enable paging
void enable_paging(page_directory_t* dir) {
    switch_page_directory(dir);
    
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;  // Set PG bit
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
}

// Memory statistics
void get_memory_stats(memory_stats_t* stats) {
    if (!stats) return;
    
    stats->total_frames = total_frames;
    stats->used_frames = used_frames;
    stats->free_frames = total_frames - used_frames;
    stats->page_faults = memory_stats.page_faults;
    stats->cow_faults = memory_stats.cow_faults;
    stats->peak_usage = memory_stats.peak_usage;
}
