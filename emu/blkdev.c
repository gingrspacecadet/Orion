#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include "blkdev.h"
#include "isa.h"


static bool pread_exact(int fd, void *buffer, size_t length, off_t offset) {
    uint8_t *p = buffer;

    while (length != 0) {
        size_t chunk = length > SSIZE_MAX
            ? (size_t)SSIZE_MAX
            : length;

        ssize_t n = pread(fd, p, chunk, offset);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }

        if (n == 0)
            return false;

        p += n;
        length -= (size_t)n;
        offset += n;
    }

    return true;
}

static bool pwrite_exact(int fd, const void *buffer, size_t length, off_t offset) {
    const uint8_t *p = buffer;

    while (length != 0) {
        size_t chunk = length > SSIZE_MAX
            ? (size_t)SSIZE_MAX
            : length;

        ssize_t n = pwrite(fd, p, chunk, offset);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }

        if (n == 0)
            return false;

        p += n;
        length -= (size_t)n;
        offset += n;
    }

    return true;
}

static bool blkdev_validate_disk_range(const Blkdev *blkdev, uint32_t lba, uint32_t sector_count) {
    if (sector_count == 0)
        return false;

    uint64_t capacity = blkdev->image_size / 512;

    if (lba > capacity)
        return false;

    return sector_count <= capacity - lba;
}

static bool blkdev_validate_dma(const Blkdev *blkdev, uint32_t address, uint64_t length) {
    if (address < RAM || length == 0)
        return false;

    uint64_t end = (uint64_t)address + length;

    if (end > (UINT64_C(1) << 32))
        return false;

    for (int i = 0; i < blkdev->mmio->device_count; i++) {
        const Device *d = &blkdev->mmio->devices[i];

        if (d->base_addr == 0 || d->size == 0)
            continue;

        uint64_t device_start = d->base_addr;
        uint64_t device_end = device_start + d->size;

        if ((uint64_t)address < device_end && device_start < end)
            return false;
    }

    return true;
}

static bool blkdev_transfer_read(Blkdev *blkdev) {
    if (!blkdev_validate_disk_range(blkdev, blkdev->active_lba, blkdev->active_sector_count)) return false;

    uint64_t offset = blkdev->active_lba * UINT64_C(512);
    uint64_t length = blkdev->active_sector_count * UINT64_C(512);

    uint8_t *buffer = malloc(length);
    if (!buffer) return false;

    if (!pread_exact(blkdev->image_fd, buffer, length, offset)) {
        free(buffer);
        return false;
    }

    memcpy(&blkdev->mmio->mem[blkdev->active_mem_addr], buffer, length);

    free(buffer);

    return true;
}

static bool blkdev_transfer_write(Blkdev *blkdev) {
    if (!blkdev_validate_disk_range(blkdev, blkdev->active_lba, blkdev->active_sector_count)) return false;

    uint64_t offset = blkdev->active_lba * UINT64_C(512);
    uint64_t length = blkdev->active_sector_count * UINT64_C(512);

    if (!pwrite_exact(blkdev->image_fd, &blkdev->mmio->mem[blkdev->active_mem_addr], length, offset)) return false;

    return true;
}

uint32_t blkdev_read(void *state, uint32_t offset, uint8_t size) {
    Blkdev *blkdev = state;

    if (size != 4)
        return 0;

    switch (offset) {
        case BLKDEV_STATUS: return blkdev->status;
        case BLKDEV_LBA: return blkdev->lba;
        case BLKDEV_MEM_ADDR: return blkdev->mem_addr;
        case BLKDEV_SECTOR_COUNT: return blkdev->sector_count;
    }

    return 0;
}

static void blkdev_set_error(Blkdev *blkdev) {
    blkdev->status |= BLKDEV_STATUS_ERROR
                   |  BLKDEV_STATUS_IRQ_PENDING;

    irq_raise(blkdev->irq);
}

void blkdev_write(void *state, uint32_t offset, uint32_t value, uint8_t size) {
    Blkdev *blkdev = state;

    if (size != 4)
        return;

    switch (offset) {
        case BLKDEV_COMMAND: {
            if (blkdev->status & BLKDEV_STATUS_BUSY) {
                blkdev_set_error(blkdev);
                return;
            }

            if (!blkdev_validate_dma(blkdev, blkdev->mem_addr, blkdev->sector_count * UINT64_C(512))) {
                blkdev_set_error(blkdev);
                return;
            }

            blkdev->active_command = value;
            blkdev->active_lba = blkdev->lba;
            blkdev->active_mem_addr = blkdev->mem_addr;
            blkdev->active_sector_count = blkdev->sector_count;

            blkdev->status &= ~BLKDEV_STATUS_ERROR;
            blkdev->status |= BLKDEV_STATUS_BUSY;

            timer_mod(blkdev->timer_pool, &blkdev->completion_timer, blkdev->timer_pool->virtual_time + 10000);
            break;
        }

        case BLKDEV_LBA: {
            blkdev->lba = value;
            break;
        }

        case BLKDEV_MEM_ADDR: {
            blkdev->mem_addr = value;
            break;
        }

        case BLKDEV_SECTOR_COUNT: {
            blkdev->sector_count = value;
            break;
        }

        case BLKDEV_INTERRUPT_ACK: {
            blkdev->status &= ~BLKDEV_STATUS_IRQ_PENDING;
            irq_lower(blkdev->irq);
            break;
        }
    }
}

static void blkdev_complete(void *state) {
    Blkdev *blkdev = state;

    bool success;

    switch (blkdev->active_command) {
        case 1:
            success = blkdev_transfer_read(blkdev);
            break;

        case 2:
            success = blkdev_transfer_write(blkdev);
            break;

        default:
            success = false;
            break;
    }

    blkdev->status &= ~BLKDEV_STATUS_BUSY;

    if (!success)
        blkdev->status |= BLKDEV_STATUS_ERROR;

    blkdev->status |= BLKDEV_STATUS_IRQ_PENDING;

    irq_raise(blkdev->irq);
}

bool blkdev_init(Blkdev *blkdev, const char *image_path) {
    if (!blkdev || !image_path)
        return false;

    int fd = open(image_path, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", __func__, strerror(errno));
        return false;
    }

    struct stat st;

    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0 || (st.st_size % 512) != 0) {
        close(fd);
        return false;
    }

    blkdev->image_fd = fd;
    blkdev->image_size = (uint64_t)st.st_size;

    timer_init(&blkdev->completion_timer, blkdev_complete, blkdev);

    return true;
}

void blkdev_destroy(Blkdev *blkdev) {
    timer_del(blkdev->timer_pool, &blkdev->completion_timer);
    close(blkdev->image_fd);
}