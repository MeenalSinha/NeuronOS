// NeuronOS NeuronFS Public Header
// fs/neuronfs.h

#ifndef NEURONFS_H
#define NEURONFS_H

#include <stdint.h>
#include <stdbool.h>

// Matches the API in fs/fs.h — extended with create/read/write 
int      fs_init(void);
void     fs_get_stats(uint64_t* reads, uint64_t* writes, uint32_t* free_blocks);

// Extended file operations (used by syscall.c)
uint32_t fs_open(const char* path);
uint32_t fs_create(const char* path, uint32_t mode);
int      fs_read(uint32_t inode, void* buf, uint32_t len, uint32_t offset);
int      fs_write(uint32_t inode, const void* buf, uint32_t len, uint32_t offset);
int      fs_close(uint32_t inode);
int      fs_unlink(const char* path);

#endif // NEURONFS_H
