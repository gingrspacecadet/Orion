#ifndef BLKDEV_H
#define BLKDEV_H

#include <stdint.h>

#include "mmio.h"
#include "timer.h"
#include "irq.h"

typedef struct {
    Mmio *mmio;

    int image_fd;
    uint64_t image_size;

    TimerPool *timer_pool;
    Timer completion_timer;
    IrqLine irq;

    uint32_t lba;
    uint32_t mem_addr;
    uint32_t sector_count;
    uint32_t status;

    uint32_t active_command;
    uint32_t active_lba;
    uint32_t active_mem_addr;
    uint32_t active_sector_count;
} Blkdev;

uint32_t blkdev_read(void *state, uint32_t offset, uint8_t size);
void blkdev_write(void *state, uint32_t offset, uint32_t value, uint8_t size);
bool blkdev_init(Blkdev *blkdev, const char *image_path);
void blkdev_destroy(Blkdev *blkdev);

#endif