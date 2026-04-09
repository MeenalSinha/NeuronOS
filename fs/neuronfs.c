// NeuronFS - Custom Journaled Filesystem
// fs/neuronfs.c

#include "../kernel/kernel.h"
#include "../drivers/ahci.h"
#include <stdint.h>
#include <stdbool.h>

#define BLOCK_SIZE 4096
#define INODE_SIZE 128
#define MAX_INODES 512
#define MAX_BLOCKS 4096
#define JOURNAL_SIZE 32

// Filesystem superblock
typedef struct {
    uint32_t magic;           // 0x4E45524F ('NERO')
    uint32_t version;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint32_t total_inodes;
    uint32_t free_inodes;
    uint32_t root_inode;
    uint32_t journal_start;
    uint32_t journal_size;
    uint8_t uuid[16];
} superblock_t;

// Inode structure
typedef struct {
    uint32_t inode_num;
    uint32_t mode;            // Permissions and type
    uint32_t uid;
    uint32_t gid;
    uint32_t size;
    uint32_t atime;           // Access time
    uint32_t mtime;           // Modification time
    uint32_t ctime;           // Creation time
    uint32_t links_count;
    uint32_t blocks_count;
    uint32_t direct_blocks[12];
    uint32_t indirect_block;
    uint32_t double_indirect;
    uint32_t triple_indirect;
} inode_t;

// Directory entry
typedef struct {
    uint32_t inode_num;
    uint16_t entry_length;
    uint8_t name_length;
    uint8_t file_type;
    char name[256];
} dir_entry_t;

// Journal entry
typedef struct {
    uint32_t transaction_id;
    uint32_t type;            // CREATE, DELETE, WRITE, etc.
    uint32_t inode_num;
    uint32_t block_num;
    uint8_t data[BLOCK_SIZE - 16];
} journal_entry_t;

// Filesystem state
static struct {
    bool mounted;
    superblock_t superblock;
    uint8_t* block_bitmap;
    uint8_t* inode_bitmap;
    inode_t* inode_table;
    
    // Journal
    journal_entry_t* journal;
    uint32_t journal_head;
    uint32_t journal_tail;
    uint32_t next_transaction_id;
    
    // Statistics
    uint64_t reads;
    uint64_t writes;
    uint64_t journal_commits;
    
} fs_state;

// Initialize filesystem
int fs_init(void) {
    // Read superblock from disk (block 0)
    superblock_t* sb = &fs_state.superblock;
    
    // For now, format a new filesystem
    sb->magic = 0x4E45524F;
    sb->version = 1;
    sb->block_size = BLOCK_SIZE;
    sb->total_blocks = MAX_BLOCKS;
    sb->free_blocks = MAX_BLOCKS - 100;
    sb->total_inodes = MAX_INODES;
    sb->free_inodes = MAX_INODES - 1;
    sb->root_inode = 1;
    sb->journal_start = MAX_BLOCKS - JOURNAL_SIZE;
    sb->journal_size = JOURNAL_SIZE;
    
    // Allocate bitmaps
    kernel_log("FS", "Allocating bitmaps", 0);
    fs_state.block_bitmap = (uint8_t*)kmalloc(MAX_BLOCKS / 8);
    fs_state.inode_bitmap = (uint8_t*)kmalloc(MAX_INODES / 8);
    if (!fs_state.block_bitmap || !fs_state.inode_bitmap) kernel_panic("FS: Bitmap allocation failed");
    memset(fs_state.block_bitmap, 0, MAX_BLOCKS / 8);
    memset(fs_state.inode_bitmap, 0, MAX_INODES / 8);
    
    // Allocate inode table
    kernel_log("FS", "Allocating inode table", 0);
    fs_state.inode_table = (inode_t*)kmalloc(sizeof(inode_t) * MAX_INODES);
    if (!fs_state.inode_table) kernel_panic("FS: Inode table allocation failed");
    memset(fs_state.inode_table, 0, sizeof(inode_t) * MAX_INODES);
    
    fs_state.journal_head = 0;
    fs_state.journal_tail = 0;
    fs_state.next_transaction_id = 1;
    
    // Create root directory
    inode_t* root = &fs_state.inode_table[sb->root_inode];
    root->inode_num = sb->root_inode;
    root->mode = 0755 | 0x4000;  // Directory
    root->uid = 0;
    root->gid = 0;
    root->size = 0;
    root->links_count = 2;
    
    // Mark root inode as used
    fs_state.inode_bitmap[sb->root_inode / 8] |= (1 << (sb->root_inode % 8));
    
    fs_state.mounted = true;
    return 0;
}

// Allocate a free block
static uint32_t alloc_block(void) {
    for (uint32_t i = 0; i < fs_state.superblock.total_blocks; i++) {
        uint32_t byte_idx = i / 8;
        uint32_t bit_idx = i % 8;
        
        if (!(fs_state.block_bitmap[byte_idx] & (1 << bit_idx))) {
            fs_state.block_bitmap[byte_idx] |= (1 << bit_idx);
            fs_state.superblock.free_blocks--;
            return i;
        }
    }
    return 0xFFFFFFFF;  // No free blocks
}

// Free a block
static void free_block(uint32_t block_num) {
    uint32_t byte_idx = block_num / 8;
    uint32_t bit_idx = block_num % 8;
    fs_state.block_bitmap[byte_idx] &= ~(1 << bit_idx);
    fs_state.superblock.free_blocks++;
}

// Allocate a free inode
static uint32_t alloc_inode(void) {
    for (uint32_t i = 1; i < fs_state.superblock.total_inodes; i++) {
        uint32_t byte_idx = i / 8;
        uint32_t bit_idx = i % 8;
        
        if (!(fs_state.inode_bitmap[byte_idx] & (1 << bit_idx))) {
            fs_state.inode_bitmap[byte_idx] |= (1 << bit_idx);
            fs_state.superblock.free_inodes--;
            return i;
        }
    }
    return 0;  // No free inodes
}

// Free an inode
static void free_inode(uint32_t inode_num) {
    uint32_t byte_idx = inode_num / 8;
    uint32_t bit_idx = inode_num % 8;
    fs_state.inode_bitmap[byte_idx] &= ~(1 << bit_idx);
    fs_state.superblock.free_inodes++;
}

// Journal operations
static void journal_commit(void);
static void journal_begin_transaction(void) {
    // Start new transaction - just increment ID
    fs_state.next_transaction_id++;
}

static void journal_log_operation(uint32_t type, uint32_t inode_num, 
                                  uint32_t block_num, void* data) {
    if ((fs_state.journal_tail + 1) % JOURNAL_SIZE == fs_state.journal_head) {
        // Journal full, commit oldest transactions
        journal_commit();
    }
    
    journal_entry_t* entry = &fs_state.journal[fs_state.journal_tail];
    entry->transaction_id = fs_state.next_transaction_id;
    entry->type = type;
    entry->inode_num = inode_num;
    entry->block_num = block_num;
    
    if (data) {
        memcpy(entry->data, data, BLOCK_SIZE - 16);
    }
    
    fs_state.journal_tail = (fs_state.journal_tail + 1) % JOURNAL_SIZE;
}

static void journal_commit(void) {
    // Write journal entries to disk
    // In a real implementation, this would flush to physical storage
    fs_state.journal_commits++;
    
    // For now, just mark journal as committed by moving head
    fs_state.journal_head = fs_state.journal_tail;
}

// Create file
int fs_create(const char* path, uint32_t mode) {
    if (!fs_state.mounted) return -1;
    
    journal_begin_transaction();
    
    // Allocate inode
    uint32_t inode_num = alloc_inode();
    if (!inode_num) return -1;
    
    inode_t* inode = &fs_state.inode_table[inode_num];
    inode->inode_num = inode_num;
    inode->mode = mode;
    inode->uid = 0;  // TODO: Get from current process
    inode->gid = 0;
    inode->size = 0;
    inode->links_count = 1;
    
    // Get current time (simplified)
    uint32_t current_time = 0;  // TODO: Implement timer_get_time()
    inode->atime = current_time;
    inode->mtime = current_time;
    inode->ctime = current_time;
    
    // Log creation to journal
    journal_log_operation(1, inode_num, 0, inode);  // Type 1 = CREATE
    journal_commit();
    
    return inode_num;
}

// Write to file
int fs_write(uint32_t inode_num, const void* data, uint32_t size, uint32_t offset) {
    if (!fs_state.mounted || inode_num >= fs_state.superblock.total_inodes) {
        return -1;
    }
    
    journal_begin_transaction();
    
    inode_t* inode = &fs_state.inode_table[inode_num];
    
    // Calculate blocks needed
    uint32_t start_block = offset / BLOCK_SIZE;
    uint32_t end_block = (offset + size - 1) / BLOCK_SIZE;
    
    const uint8_t* src = (const uint8_t*)data;
    uint32_t written = 0;
    
    for (uint32_t block_idx = start_block; block_idx <= end_block; block_idx++) {
        // Get or allocate block
        uint32_t block_num;
        
        if (block_idx < 12) {
            // Direct block
            if (!inode->direct_blocks[block_idx]) {
                inode->direct_blocks[block_idx] = alloc_block();
                if (!inode->direct_blocks[block_idx]) return written;
            }
            block_num = inode->direct_blocks[block_idx];
        } else {
            // Would implement indirect blocks here
            return written;
        }
        
        // Calculate offset within block
        uint32_t block_offset = (block_idx == start_block) ? (offset % BLOCK_SIZE) : 0;
        uint32_t block_size = BLOCK_SIZE - block_offset;
        if (written + block_size > size) {
            block_size = size - written;
        }
        
        // Memory block structure
        uint8_t* block_data = (uint8_t*)kmalloc(BLOCK_SIZE);
        
        // Read existing block via AHCI if we are partially writing
        if (block_size < BLOCK_SIZE) {
            ahci_read(block_num * (BLOCK_SIZE / 512), (BLOCK_SIZE / 512), block_data);
        }
        
        // Apply modifications
        memcpy(block_data + block_offset, src + written, block_size);
        
        // Execute physical AHCI write
        ahci_write(block_num * (BLOCK_SIZE / 512), (BLOCK_SIZE / 512), block_data);
        
        // Log to journal
        journal_log_operation(2, inode_num, block_num, block_data);  // Type 2 = WRITE
        
        kfree(block_data);
        written += block_size;
    }
    
    // Update inode
    if (offset + written > inode->size) {
        inode->size = offset + written;
    }
    
    journal_commit();
    fs_state.writes++;
    
    return written;
}

// Read from file
int fs_read(uint32_t inode_num, void* buffer, uint32_t size, uint32_t offset) {
    if (!fs_state.mounted || inode_num >= fs_state.superblock.total_inodes) {
        return -1;
    }
    
    inode_t* inode = &fs_state.inode_table[inode_num];
    
    if (offset >= inode->size) return 0;
    if (offset + size > inode->size) {
        size = inode->size - offset;
    }
    
    uint32_t start_block = offset / BLOCK_SIZE;
    uint32_t end_block = (offset + size - 1) / BLOCK_SIZE;
    
    uint8_t* dest = (uint8_t*)buffer;
    uint32_t read = 0;
    
    for (uint32_t block_idx = start_block; block_idx <= end_block; block_idx++) {
        uint32_t block_num;
        
        if (block_idx < 12) {
            block_num = inode->direct_blocks[block_idx];
        } else {
            return read;  // Indirect blocks not implemented
        }
        
        if (!block_num) return read;
        
        uint32_t block_offset = (block_idx == start_block) ? (offset % BLOCK_SIZE) : 0;
        uint32_t block_size = BLOCK_SIZE - block_offset;
        if (read + block_size > size) {
            block_size = size - read;
        }
        
        // Read from block via physical AHCI
        uint8_t* block_data = (uint8_t*)kmalloc(BLOCK_SIZE);
        ahci_read(block_num * (BLOCK_SIZE / 512), (BLOCK_SIZE / 512), block_data);
        
        memcpy(dest + read, block_data + block_offset, block_size);
        kfree(block_data);
        
        read += block_size;
    }
    
    fs_state.reads++;
    return read;
}

// Delete file
int fs_delete(uint32_t inode_num) {
    if (!fs_state.mounted || inode_num >= fs_state.superblock.total_inodes) {
        return -1;
    }
    
    journal_begin_transaction();
    
    inode_t* inode = &fs_state.inode_table[inode_num];
    
    // Free all blocks
    for (int i = 0; i < 12; i++) {
        if (inode->direct_blocks[i]) {
            free_block(inode->direct_blocks[i]);
        }
    }
    
    // Free inode
    free_inode(inode_num);
    
    // Log deletion
    journal_log_operation(3, inode_num, 0, NULL);  // Type 3 = DELETE
    journal_commit();
    
    return 0;
}

// Get filesystem statistics
void fs_get_stats(uint64_t* reads, uint64_t* writes, uint32_t* free_blocks) {
    if (reads) *reads = fs_state.reads;
    if (writes) *writes = fs_state.writes;
    if (free_blocks) *free_blocks = fs_state.superblock.free_blocks;
}
