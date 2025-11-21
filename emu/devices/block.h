#ifndef DEVICES_BLOCK_H
#define DEVICES_BLOCK_H

#define SECTOR_SIZE 512

typedef struct {
    FILE* file;
    uint8_t buffer[SECTOR_SIZE];
    uint32_t sector_index; /* 24-bit supported */
    size_t buf_pos;
    uint8_t status; /* 0 idle, 1 data ready, 2 write mode, 3 error */
    int dirty;
} BlockState;

BlockState* block_init(const char* path);

extern BlockState* block_state;

extern Device block_device;

#endif