#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "../device.h"
#include "block.h"

static int seek_sector(FILE* f, uint32_t sector) {
    if (!f) return -1;
    off_t off = (off_t)sector * SECTOR_SIZE;
    if (fseeko(f, off, SEEK_SET) != 0) return -1;
    return 0;
}

uint32_t block_read(Device* self, uint32_t addr_word) {
    BlockState* b = (BlockState*)self->state;
    switch (addr_word) {
        case 0: {
            if (b->status != 1) return 0;
            uint8_t v = b->buffer[b->buf_pos];
            b->buf_pos++;
            if (b->buf_pos >= SECTOR_SIZE) {
                b->status = 0;
                b->buf_pos = 0;
            }
            return (uint32_t)v;
        }
        case 1: return (uint32_t)b->status;
        case 3: return (uint32_t)(b->sector_index & 0xFF);
        case 4: return (uint32_t)((b->sector_index >> 8) & 0xFF);
        case 5: return (uint32_t)((b->sector_index >> 16) & 0xFF);
        default: return 0;
    }
}

void block_write(Device* self, uint32_t addr_word, uint32_t value) {
    BlockState* b = (BlockState*)self->state;
    switch (addr_word) {
        case 0: {
            if (b->status != 2) return;
            if (b->buf_pos >= SECTOR_SIZE) return;
            b->buffer[b->buf_pos++] = (uint8_t)(value & 0xFF);
            b->dirty = 1;
            break;
        }
        case 2: {
            uint32_t cmd = value & 0xFF;
            if (cmd == 1) {
                b->buf_pos = 0;
                b->status = 0;
                b->dirty = 0;
                memset(b->buffer, 0, SECTOR_SIZE);
            } else if (cmd == 2) {
                if (!b->file || seek_sector(b->file, b->sector_index) != 0) {
                    b->status = 3;
                } else {
                    size_t n = fread(b->buffer, 1, SECTOR_SIZE, b->file);
                    if (n < SECTOR_SIZE) {
                        if (feof(b->file)) {
                            memset(b->buffer + n, 0, SECTOR_SIZE - n);
                            b->status = 1;
                            b->buf_pos = 0;
                        } else {
                            b->status = 3;
                        }
                    } else {
                        b->status = 1;
                        b->buf_pos = 0;
                    }
                }
                b->dirty = 0;
            } else if (cmd == 3) {
                if (!b->file || seek_sector(b->file, b->sector_index) != 0) {
                    b->status = 3;
                } else {
                    size_t w = fwrite(b->buffer, 1, SECTOR_SIZE, b->file);
                    if (w != SECTOR_SIZE) {
                        b->status = 3;
                    } else {
                        fflush(b->file);
                        b->status = 0;
                        b->dirty = 0;
                        b->buf_pos = 0;
                    }
                }
            } else if (cmd == 4) {
                if (!b->file || fflush(b->file) != 0) b->status = 3;
            }
            break;
        }
        case 3:
            b->sector_index = (b->sector_index & 0xFFFF00) | (value & 0xFF);
            break;
        case 4:
            b->sector_index = (b->sector_index & 0xFF00FF) | ((value & 0xFF) << 8);
            break;
        case 5:
            b->sector_index = (b->sector_index & 0x00FFFF) | ((value & 0xFF) << 16);
            break;
        case 6:
            b->status = 2;
            b->buf_pos = 0;
            b->dirty = 0;
            break;
        default:
            break;
    }
}

BlockState* block_init(const char* path) {
    BlockState* b = (BlockState*)calloc(1, sizeof(BlockState));
    if (!b) return NULL;
    b->file = fopen(path, "r+b");
    if (!b->file) b->file = fopen(path, "w+b");
    if (!b->file) { free(b); return NULL; }
    b->sector_index = 0;
    b->buf_pos = 0;
    b->status = 0;
    b->dirty = 0;
    memset(b->buffer, 0, SECTOR_SIZE);
    return b;
}

void block_close(BlockState* b) {
    if (!b) return;
    if (b->file) { fflush(b->file); fclose(b->file); }
    free(b);
}

BlockState* block_state = NULL;

Device block_device = {
    .read = block_read,
    .write = block_write,
    .base = 0x00FE0000,
    .size = 0x8,
    .state = NULL,
};