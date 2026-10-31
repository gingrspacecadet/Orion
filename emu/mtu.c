#include <assert.h>
#include "mtu.h"
#include "isa.h"

uint32_t mtu_read(void *state, uint32_t offset, uint8_t size) {
    if (size != 4) return 0;

    Mtu *mtu = state;

    switch (offset) {
        case MTU_FAULT_ADDR: return mtu->fault_address;
        case MTU_CONFIG: return mtu->selected_entry;
        case TLB_HI_WIN: return mtu->tlb_entries[mtu->selected_entry] >> 32;
        case TLB_LO_WIN: return mtu->tlb_entries[mtu->selected_entry] & UINT32_MAX;
        case MTU_CURR_ASID: return mtu->current_asid;
        case MTU_FAULT_STATUS: return mtu->fault_status;
        default: return 0;
    }
}

void mtu_write(void *state, uint32_t offset, uint32_t value, uint8_t size) {
    if (size != 4) return;

    Mtu *mtu = state;

    switch (offset) {
        case MTU_CONFIG: mtu->selected_entry = value & 0x1F; break;
        case TLB_HI_WIN: {
            mtu->tlb_entries[mtu->selected_entry] &= ~((uint64_t)UINT32_MAX << 32);
            mtu->tlb_entries[mtu->selected_entry] |= (uint64_t)value << 32;
            break;
        }
        case TLB_LO_WIN: {
            mtu->tlb_entries[mtu->selected_entry] &= ~UINT32_MAX;
            mtu->tlb_entries[mtu->selected_entry] |= value;
            break;
        }
        case MTU_CURR_ASID: mtu->current_asid = value & 0x3FF; break;
        default: break;
    }
}

#define ERR(reason) do { mtu->fault_address = virtual_address; mtu->fault_status = (access & 0x3) | (((reason) & 0x7) << 2); return (reason); } while (0)

MtuResult mtu_translate(Mtu *mtu, uint32_t virtual_address, MtuAccess access, bool user_mode, uint32_t *physical_address) {
    assert(mtu);

    uint64_t found = 0;

    for (int i = 0; i < 32; i++) {
        uint64_t tlb = mtu->tlb_entries[i];
        uint32_t tlb_hi = tlb >> 32;
        uint32_t tlb_lo = tlb & UINT32_MAX;

        if (!(tlb_hi & 1)) continue;

        uint32_t vpn = (tlb_hi >> 12) & 0xFFFFF;
        uint32_t asid = (tlb_hi >> 2) & 0x3FF;
        uint32_t pfn = (tlb_lo >> 12) & 0xFFFFF;

        if (vpn == ((virtual_address >> 12) & 0xFFFFF) && asid == mtu->current_asid) {
            found = tlb;
            break;
        }
    }

    if (found == 0) ERR(MTU_MISS);

    if (user_mode && !(found & (1u << 2))) ERR(MTU_PERM);

    switch (access) {
        case DATA_READ: break;
        case DATA_WRITE: 
        case ATOMIC_XCHG: {
            if (!(found & (1u << 3))) ERR(MTU_PERM);
            break;
        }

        case INSTR_FETCH: {
            if (!(found & (1u << 0))) ERR(MTU_PERM);
            break;
        }

        default: break;
    }

    uint32_t phys = (found & (0xFFFFFul << 12)) | (virtual_address & 0xFFF);

    *physical_address = phys;

    return MTU_SUCCESS;
}
