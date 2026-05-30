#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "linker.h"

static SectionLayout layout[MAX_SECTIONS];
static int layout_count = 0;

static ExecSection exec_sections[MAX_SECTIONS];
static int exec_section_count = 0;

static GlobalSymbol global_sym_table[MAX_GLOBAL_SYMBOLS];
static int global_sym_count = 0;

static DeferredReloc master_reloc_table[MAX_TOTAL_RELOCS];
static int master_reloc_count = 0;

void parse_linker_script(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Linker Error: Failed to open linker script %s\n", path);
        exit(1);
    }

    char name[32];
    uint32_t vma;

    while (fscanf(f, "%31s %x", name, &vma) == 2) {
        strcpy(layout[layout_count].name, name);
        layout[layout_count].vma = vma;
        layout_count++;
        printf("Linker: Mapped section layout %s -> 0x%08X\n", name, vma);
    }
    fclose(f);
}

uint32_t get_section_vma(const char *name) {
    for (int i = 0; i < layout_count; i++) {
        if (strcmp(layout[i].name, name) == 0) {
            return layout[i].vma;
        }
    }
    fprintf(stderr, "Linker Error: Unknown section '%s'.\n", name);
    exit(1);
}

static int find_or_create_exec_section(const char *name) {
    for (int i = 0; i < exec_section_count; i++) {
        if (strcmp(exec_sections[i].name, name) == 0) {
            return i;
        }
    }
    if (exec_section_count >= MAX_SECTIONS) {
        fprintf(stderr, "Linker Error: Maximum execution sections exceeded.\n");
        exit(1);
    }
    strncpy(exec_sections[exec_section_count].name, name, 31);
    exec_sections[exec_section_count].size = 0;
    return exec_section_count++;
}

void link_object_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Linker Error: Could not open %s\n", filename);
        exit(1);
    }

    ObjHeader header;
    if (fread(&header, sizeof(ObjHeader), 1, f) != 1 || header.magic != OBJ_MAGIC) {
        fprintf(stderr, "Linker Error: Invalid or corrupt object file %s\n", filename);
        exit(1);
    }

    int local_to_global_idx[256] = {0};
    uint32_t local_base_offsets[256] = {0};

    // 1. Read section table metadata headers
    for (uint32_t i = 0; i < header.section_count; i++) {
        ObjSectionHeader sh;
        if (fread(&sh, sizeof(ObjSectionHeader), 1, f) != 1) {
            fprintf(stderr, "Linker Error: Truncated section header in %s\n", filename);
            exit(1);
        }

        int g_idx = find_or_create_exec_section(sh.name);
        local_to_global_idx[sh.id] = g_idx;
        local_base_offsets[sh.id] = exec_sections[g_idx].size;
    }

    // Capture precise baseline offsets now that we are at the end of the headers
    long payload_start_pos = ftell(f);
    long headers_start_pos = payload_start_pos - (header.section_count * sizeof(ObjSectionHeader));

    // 2. Read dynamic binary payloads safely matching section tracking
    for (uint32_t i = 0; i < header.section_count; i++) {
        fseek(f, headers_start_pos + (i * sizeof(ObjSectionHeader)), SEEK_SET);
        ObjSectionHeader sh;
        fread(&sh, sizeof(ObjSectionHeader), 1, f);
        
        fseek(f, payload_start_pos, SEEK_SET);
        int g_idx = local_to_global_idx[sh.id];

        if (sh.size > 0) {
            if (exec_sections[g_idx].size + sh.size > MAX_EXEC_SECTION_SIZE) {
                fprintf(stderr, "Linker Error: Segment overflow merging '%s' inside %s\n", sh.name, filename);
                exit(1);
            }
            fread(&exec_sections[g_idx].buffer[exec_sections[g_idx].size], 1, sh.size, f);
            exec_sections[g_idx].size += sh.size;
            
            payload_start_pos = ftell(f);
        }
    }

    // Ensure our stream points directly to the symbol data now
    fseek(f, payload_start_pos, SEEK_SET);

    // 3. Read and store global symbols
    for (uint32_t i = 0; i < header.sym_count; i++) {
        ObjSymbol sym;
        if (fread(&sym, sizeof(ObjSymbol), 1, f) != 1) {
            fprintf(stderr, "Linker Error: Truncated symbol table in %s\n", filename);
            exit(1);
        }

        int g_idx = local_to_global_idx[sym.section];
        const char *sec_name = exec_sections[g_idx].name;

        uint32_t absolute_addr = get_section_vma(sec_name) + local_base_offsets[sym.section] + sym.offset;

        for (int g = 0; g < global_sym_count; g++) {
            if (strcmp(global_sym_table[g].name, sym.name) == 0) {
                fprintf(stderr, "Linker Error: Redefinition of symbol '%s'\n", sym.name);
                exit(1);
            }
        }

        strncpy(global_sym_table[global_sym_count].name, sym.name, 31);
        global_sym_table[global_sym_count].absolute_addr = absolute_addr;
        global_sym_count++;
    }

    // 4. Ingest and queue dynamic relocation requests
    for (uint32_t i = 0; i < header.reloc_count; i++) {
        ObjReloc r;
        if (fread(&r, sizeof(ObjReloc), 1, f) != 1) {
            fprintf(stderr, "Linker Error: Truncated relocation table in %s\n", filename);
            exit(1);
        }

        if (master_reloc_count >= MAX_TOTAL_RELOCS) {
            fprintf(stderr, "Linker Error: Too many relocations logged across compilation unit\n");
            exit(1);
        }

        int g_idx = local_to_global_idx[r.patch_section];

        master_reloc_table[master_reloc_count++] = (DeferredReloc){
            .reloc = r,
            .global_section_idx = g_idx,
            .section_base_offset = local_base_offsets[r.patch_section]
        };
    }

    fclose(f);
}

static uint32_t read_section_u32(int g_idx, uint32_t offset) {
    uint8_t *buf = exec_sections[g_idx].buffer;
    return (buf[offset]) | (buf[offset + 1] << 8) | (buf[offset + 2] << 16) | (buf[offset + 3] << 24);
}

static void write_section_u32(int g_idx, uint32_t offset, uint32_t val) {
    uint8_t *buf = exec_sections[g_idx].buffer;
    buf[offset]     = (val & 0xFF);
    buf[offset + 1] = ((val >> 8) & 0xFF);
    buf[offset + 2] = ((val >> 16) & 0xFF);
    buf[offset + 3] = ((val >> 24) & 0xFF);
}

void resolve_relocations(void) {
    for (int i = 0; i < master_reloc_count; i++) {
        DeferredReloc *def = &master_reloc_table[i];

        uint32_t target_addr = 0;
        bool found = false;

        for (int g = 0; g < global_sym_count; g++) {
            if (strcmp(global_sym_table[g].name, def->reloc.symbol_name) == 0) {
                target_addr = global_sym_table[g].absolute_addr;
                found = true;
                break;
            }
        }

        if (!found) {
            fprintf(stderr, "Linker Error: Undefined reference to '%s'\n", def->reloc.symbol_name);
            exit(1);
        }

        int g_idx = def->global_section_idx;
        const char *sec_name = exec_sections[g_idx].name;

        uint32_t patch_offset = def->section_base_offset + def->reloc.patch_offset;
        uint32_t instr_addr = get_section_vma(sec_name) + patch_offset;
        uint32_t instr = read_section_u32(g_idx, patch_offset);

        if (def->reloc.patch_type == RELOC_PC_REL) {
            int32_t offset = (int32_t)(target_addr - instr_addr);
            
            if (offset < -32768 || offset > 32767) {
                fprintf(stderr, "Linker Error: PC-relative branch to '%s' is out of 16-bit range! (Delta: %d bytes)\n", 
                        def->reloc.symbol_name, offset);
                exit(1);
            }
            
            uint32_t imm16 = ((uint32_t)offset) & 0xFFFF;
            instr = (instr & ~0x0003FFFC) | (imm16 << 2);
        }
        else if (def->reloc.patch_type == RELOC_HI16) {
            uint32_t hi16 = (target_addr >> 16) & 0xFFFF;
            instr = (instr & ~0x0003FFFC) | (hi16 << 2);
        }
        else if (def->reloc.patch_type == RELOC_LO16) {
            uint32_t lo16 = target_addr & 0xFFFF;
            instr = (instr & ~0x0003FFFC) | (lo16 << 2);
        }
        else if (def->reloc.patch_type == RELOC_32) {
            instr = target_addr;
        }
        else {
            fprintf(stderr, "Linker Error: Unknown relocation type %d\n", def->reloc.patch_type);
            exit(1);
        }

        write_section_u32(g_idx, patch_offset, instr);
    }
}

void write_executable(const char *out_path) {
    FILE *f = fopen(out_path, "wb");
    if (!f) {
        fprintf(stderr, "Linker Error: Could not open output binary %s\n", out_path);
        exit(1);
    }

    // 1. Sort sections by VMA layout mapping (Bubble Sort)
    for (int i = 0; i < exec_section_count - 1; i++) {
        for (int j = 0; j < exec_section_count - i - 1; j++) {
            if (get_section_vma(exec_sections[j].name) > get_section_vma(exec_sections[j + 1].name)) {
                ExecSection temp = exec_sections[j];
                exec_sections[j] = exec_sections[j + 1];
                exec_sections[j + 1] = temp;
            }
        }
    }

    // Find the absolute base VMA of our executable binary image
    uint32_t binary_base_vma = 0xFFFFFFFF;
    for (int i = 0; i < exec_section_count; i++) {
        if (exec_sections[i].size > 0) {
            uint32_t vma = get_section_vma(exec_sections[i].name);
            if (vma < binary_base_vma) {
                binary_base_vma = vma;
            }
        }
    }

    if (binary_base_vma == 0xFFFFFFFF) {
        fprintf(stderr, "Linker Error: No sections containing payload data found.\n");
        fclose(f);
        return;
    }

    printf("Linker: Binary base image VMA established at 0x%08X\n", binary_base_vma);

    // 2. Write out sections, calculating explicit file offsets
    for (int i = 0; i < exec_section_count; i++) {
        if (exec_sections[i].size == 0) continue;

        uint32_t section_vma = get_section_vma(exec_sections[i].name);
        
        // Calculate exactly where this section belongs relative to the file start
        long expected_file_offset = (long)(section_vma - binary_base_vma);
        long current_file_offset = ftell(f);

        if (expected_file_offset < current_file_offset) {
            fprintf(stderr, "Linker Error: Section '%s' (VMA: 0x%08X) overlaps with previous file allocations!\n", 
                    exec_sections[i].name, section_vma);
            exit(1);
        }

        // Pad up to the exact file offset required
        if (expected_file_offset > current_file_offset) {
            long padding_needed = expected_file_offset - current_file_offset;
            uint8_t zero = 0;
            for (long p = 0; p < padding_needed; p++) {
                fwrite(&zero, 1, 1, f);
            }
        }

        // Write the consolidated payload block cleanly
        printf(" -> Writing section '%s' at file offset 0x%08lX (%u bytes)\n", 
               exec_sections[i].name, ftell(f), exec_sections[i].size);
               
        fwrite(exec_sections[i].buffer, 1, exec_sections[i].size, f);
    }

    fclose(f);
    printf("Linker Execution complete. Absolute binary written safely.\n");
}