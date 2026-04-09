// NeuronOS fs.h
#ifndef FS_H
#define FS_H

#include <stdint.h>

int fs_init(void);
void fs_get_stats(uint64_t* reads, uint64_t* writes, uint32_t* free_blocks);

#endif // FS_H
