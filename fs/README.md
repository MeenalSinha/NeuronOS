# Filesystem - NeuronFS

NeuronFS is a simple journaled filesystem designed for NeuronOS with AI-awareness built in.

## Features

- **Journaling:** Transaction log for crash consistency
- **Backend:** Physical AHCI/SATA storage (non-Hardware-Mapped)
- **Inodes:** 4096 inodes, 65536 blocks (4KB each)
- **Operations:** create, read, write, delete
- **Snapshots:** Framework for filesystem snapshots
- **AI Integration:** Access pattern tracking for predictive prefetch

## Structure

```
Superblock (block 0):
- Magic number: 0x4E46530A ('NFS\n')
- Block size: 4096 bytes
- Total blocks: 65536
- Free blocks
- Inode count: 4096

Inode Table (blocks 1-32):
- 128 inodes per block
- File metadata, block pointers

Journal (blocks 33-1056):
- 1024 journal entries
- Transaction logging
- Crash recovery

Data Blocks (blocks 1057+):
- Actual file data
- Allocated on demand
```

## AI Features

NeuronFS tracks file access patterns to enable AI-driven prefetching:

```c
// Track access sequence
file_access_pattern[pid][seq_idx++] = file_id;

// When pattern A→B→C detected consistently
// AI prefetches C when B is accessed
```

## Operations

```c
uint32_t fs_create(const char* path, uint16_t mode);
int fs_read(uint32_t inode, void* buffer, size_t size, off_t offset);
int fs_write(uint32_t inode, const void* buffer, size_t size, off_t offset);
int fs_delete(uint32_t inode);
```

## Persistence

AI models are stored in NeuronFS:
- `/ai_models.dat` - Serialized ML models
- `/ai_decisions.log` - Decision audit trail

## Build

Integrated into kernel build:
```bash
make kernel  # Includes NeuronFS
```
