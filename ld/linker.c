#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "linker.h"

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

    uint32_t current_text_base = exec_text_size;
    uint32_t current_data_base = exec_data_size;

    if (header.text_size > 0) {
        if (exec_text_size + header.text_size > MAX_EXEC_SIZE) {
            fprintf(stderr, "Linker Error: .text section overflow\n");
            exit(1);
        }
        fread(&exec_text[exec_text_size], 1, header.text_size, f);
        exec_text_size += header.text_size;
    }

    if (header.data_size > 0) {
        if (exec_data_size + header.data_size > MAX_EXEC_SIZE) {
            fprintf(stderr, "Linker Error: .data section overflow\n");
            exit(1);
        }
        fread(&exec_data[exec_data_size], 1, header.data_size, f);
        exec_data_size += header.data_size;
    }

    for (uint32_t i = 0; i < header.sym_count; i++) {
        ObjSymbol sym;
        fread(&sym, sizeof(ObjSymbol), 1, f);

        uint32_t absolute_addr = 0;
        if (sym.section == 1) {
            absolute_addr = text_vma + current_text_base + sym.offset;
        } else if (sym.section == 2) {
            absolute_addr = data_vma + current_data_base + sym.offset;
        } else {
            fprintf(stderr, "Linker Error: Unknown section %d for symbol %s\n", sym.section, sym.name);
            exit(1);
        }

        for (int g = 0; g < global_sym_count; g++) {
            if (strcmp(global_sym_table[g].name, sym.name) == 0) {
                fprintf(stderr, "Linker Error: Redefinition of symbol %s\n", sym.name);
                exit(1);
            }
        }

        strncpy(global_sym_table[global_sym_count].name, sym.name, 31);
        global_sym_table[global_sym_count].absolute_addr = absolute_addr;
        global_sym_count++;
    }

    for (uint32_t i = 0; i < header.reloc_count; i++) {
        ObjReloc r;
        fread(&r, sizeof(ObjReloc), 1, f);

        if (master_reloc_count >= MAX_TOTAL_RELOCS) {
            fprintf(stderr, "Linker Error: Too many relocations\n");
            exit(1);
        }

        master_reloc_table[master_reloc_count++] = (DeferredReloc){
            .reloc = r,
            .text_base_offset = current_text_base
        };
    }

    fclose(f);
}

static uint32_t read_exec_u32(uint32_t offset) {
    return (exec_text[offset]) |
           (exec_text[offset + 1] << 8) |
           (exec_text[offset + 2] << 16) |
           (exec_text[offset + 3] << 24);
}

static void write_exec_u32(uint32_t offset, uint32_t val) {
    exec_text[offset]     = (val & 0xFF);
    exec_text[offset + 1] = ((val >> 8) & 0xFF);
    exec_text[offset + 2] = ((val >> 16) & 0xFF);
    exec_text[offset + 3] = ((val >> 24) & 0xFF);
}

void resolve_relocations(void) {
    int resolved_count = 0;

    for (int i = 0; i < master_reloc_count; i++) {
        DeferredReloc *def = &master_reloc_table[i];

        uint32_t target_addr = 0;
        bool found = 0;

        for (int g = 0; g < global_sym_count; g++) {
            if (strcmp(global_sym_table[g].name, def->reloc.symbol_name) == 0) {
                target_addr = global_sym_table[g].absolute_addr;
                found = true;
                break;
            }
        }

        if (!found) {
            fprintf(stderr, "Linker Error: Undefined reference to %s\n", def->reloc.symbol_name);
            exit(1);
        }

        uint32_t patch_offset = def->text_base_offset + def->reloc.patch_offset;
        uint32_t instr_addr = text_vma + patch_offset;
        uint32_t instr = read_exec_u32(patch_offset);
        if (def->reloc.patch_type == RELOC_PC_REL) {
            int32_t offset = (int32_t)(target_addr - instr_addr);
            
            if (offset < -32768 || offset > 32767) {
                fprintf(stderr, "Linker Error: PC-relative jump to '%s' is out of 16-bit range! (Delta: %d bytes)\n", 
                        def->reloc.symbol_name, offset);
                exit(1);
            }
            
            uint32_t imm16 = ((uint32_t)offset) & 0xFFFF;
            instr = (instr & ~0x0003FFFC) | (imm16 << 2);
            
            if ((instr & 0x001C0000) != 0) {
                fprintf(stderr, "Linker Internal Error: Relocation corrupted instruction reserved bits!\n");
                exit(1);
            }
        }
        else if (def->reloc.patch_type == RELOC_HI16) {
            uint32_t hi16 = (target_addr >> 16) & 0xFFFF;
            instr = (instr & ~0x0003FFFC) | (hi16 << 2);
        }
        else if (def->reloc.patch_type == RELOC_LO16) {
            uint32_t lo16 = target_addr & 0xFFFF;
            instr = (instr & ~0x0003FFFC) | (lo16 << 2);
        }
        else {
            fprintf(stderr, "Linker Error: Unknown relocation type %d\n", def->reloc.patch_type);
            exit(1);
        }

        write_exec_u32(patch_offset, instr);
        resolved_count++;
    }
}

void write_executable(const char *out_path) {
    FILE *f = fopen(out_path, "wb");
    if (!f) {
        fprintf(stderr, "Linker Error: Could not open output file %s\n", out_path);
        exit(1);
    }

    if (exec_text_size > 0) {
        fwrite(exec_text, 1, exec_text_size, f);
    }

    if (exec_data_size > 0) {
        uint32_t expected_data_start = text_vma + exec_text_size;

        if (data_vma < expected_data_start) {
            fprintf(stderr, "Linker Error: .data WMA (0x%08X) overlaps with .text segment (ends at 0x%08X)\n", data_vma, expected_data_start);
            exit(1);
        }

        if (data_vma > expected_data_start) {
            uint32_t padding_bytes = data_vma - expected_data_start;

            uint8_t zero = 0;
            for (uint32_t i = 0; i < padding_bytes; i++) {
                fwrite(&zero, 1, 1, f);
            }
        }

        fwrite(exec_data, 1, exec_data_size, f);
    }

    fclose(f);
}