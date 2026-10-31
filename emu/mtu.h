#ifndef MTU_H
#define MTU_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint64_t tlb_entries[32];
    uint8_t selected_entry;
    uint32_t current_asid;
    uint32_t fault_address;
    uint32_t fault_status;
} Mtu;

uint32_t mtu_read(void *state, uint32_t offset, uint8_t size);
void mtu_write(void *state, uint32_t offset, uint32_t value, uint8_t size);

typedef enum {
    INSTR_FETCH,
    DATA_READ,
    DATA_WRITE,
    ATOMIC_XCHG
} MtuAccess;

typedef enum {
    MTU_SUCCESS,
    MTU_MISS,
    MTU_PERM,
} MtuResult;

MtuResult mtu_translate(Mtu *mtu, uint32_t virtual_address, MtuAccess access, bool user_mode, uint32_t *physical_address);

#endif