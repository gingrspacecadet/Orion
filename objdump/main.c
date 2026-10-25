#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#define X_RELOC_TYPE_IMPL
#include "obj.h"
#include "isa.h"

#define X(NAME) #NAME,
const char *const opcode_names[] = {
    X_OPCODES
};
#undef X

const struct {Opcodes code; OpcodeTypes type;}
opcodes[] = {
    [OP_ADD] = {OP_ADD, TYPE_A},
    [OP_SUB] = {OP_SUB, TYPE_A},
    [OP_MUL] = {OP_MUL, TYPE_A},
    [OP_DIV] = {OP_DIV, TYPE_A},
    [OP_DIVU] = {OP_DIVU, TYPE_A},
    [OP_NOP] = {OP_NOP, TYPE_X},
    [OP_LDR] = {OP_LDR, TYPE_M},
    [OP_LUI] = {OP_LUI, TYPE_A},
    [OP_CALL] = {OP_CALL, TYPE_J},
};

static void print_instr(uint32_t word) {
    printf("%s ", opcode_names[OPCODE(word)]);
    switch (opcodes[OPCODE(word)].type) {
        case TYPE_A:
        case TYPE_M: {
            printf("r%u, r%u, ", A_RD(word), A_RN(word));
            if (A_REGISTER(word))
                printf("r%u", A_OPERAND(word) >> 11);
            else if ((int16_t)A_OPERAND(word) < 0)
                printf("#%u (%d)", A_OPERAND(word), (int16_t)A_OPERAND(word));
            else
                printf("#%u", A_OPERAND(word));
                
            break;
        }

        case TYPE_X: {
            break;
        }

        case TYPE_J: {
            uint32_t disp = J_DISPLACEMENT(word);

            int32_t signed_disp = (disp & (1u << 25)) ? (int32_t)(disp | ~((1u << 26) - 1)) : (int32_t)disp;
            
            printf(" #%u", disp);

            if (signed_disp < 0)
                printf(" (%d)", signed_disp);
            break;
        }

        default: {
            break;
        }
    }
}

void print_obj(FILE *f) {
    obj_header_t header;
    fread(&header, sizeof(obj_header_t), 1, f);

    printf("== OrionObj v%d.%d.%d ==\n", (uint8_t)(header.version >> 24), (uint8_t)(header.version >> 16), (uint8_t)(header.version >> 8));
    printf("Sections    - %d\n", header.sections);
    printf("Symbols     - %d\n", header.symbols);
    printf("Relocations - %d\n", header.relocs);
    putchar('\n');

    obj_section_t sections[header.sections];
    obj_symbol_t symbols[header.symbols];
    obj_reloc_t relocs[header.relocs];

    fread(sections, sizeof(obj_section_t), header.sections, f);
    fread(symbols, sizeof(obj_symbol_t), header.symbols, f);
    fread(relocs, sizeof(obj_reloc_t), header.relocs, f);

    uint8_t data_pool[header.data_pool_size];
    fread(data_pool, sizeof(uint8_t), header.data_pool_size, f);

    uint8_t string_pool[header.string_pool_size];
    fread(string_pool, sizeof(uint8_t), header.string_pool_size, f);

    for (uint32_t i = 0; i < header.symbols; i++) {
        obj_symbol_t symbol = symbols[i];
        printf("Symbol %d \"%s\":\n", i + 1, &string_pool[symbol.name]);
        printf("  - offset = 0x%08X\n", symbol.offset);
        printf("  - section = %d", symbol.section);
        if (symbol.section != 0)  printf(" (\"%s\")", &string_pool[sections[symbol.section - 1].name]);
        putchar('\n');
        printf("  - flags  = %c\n", symbol.flags & SYMFLAG_GLOBAL ? 'g' : '-');
        putchar('\n');
    }

    for (uint32_t i = 0; i < header.sections; i++) {
        obj_section_t section = sections[i];
        printf("Section %d \"%s\":\n", i + 1, &string_pool[section.name]);
        printf("  - align = %u\n", section.align);
        printf("  - size  = %u\n", section.size);
        printf("  - flags = %c%c%c%c\n", section.flags & 128 ? 'r' : '-', section.flags & 64 ? 'w': '-', section.flags & 32 ? 'x' : '-', section.flags & 16 ? 'u' : '-');

        printf("\n  Instructions:\n");
        for (uint32_t j = 0; j < section.size; j += 4) {
            uint32_t word;
            memcpy(&word, &data_pool[section.data + j], sizeof(word));
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            word = __builtin_bswap32(word);
#endif
            
            printf("    ");
            print_instr(word);
            for (uint32_t k = 0; k < header.relocs; k++) {
                if (relocs[k].section == section.id && relocs[k].offset == j) {
                    printf("\t; %s %s%+" PRId32, reloc_type_names[relocs[k].type], &string_pool[symbols[relocs[k].symbol].name], relocs[k].addend);
                }
            }
            putchar('\n');
        }
    }
}

void print_exe(FILE *f) {
    exe_header_t header;
    fread(&header, sizeof(exe_header_t), 1, f);

    exe_section_t sections[header.sections];
    fread(sections, sizeof(exe_section_t), header.sections, f);

    printf("== OrionExe v%d.%d.%d ==\n", (uint8_t)(header.version >> 24), (uint8_t)(header.version >> 16), (uint8_t)(header.version >> 8));
    printf("Entry    - 0x%08X\n", header.entry);
    printf("Sections - %d\n", header.sections);
    putchar('\n');

    for (uint32_t i = 0; i < header.sections; i++) {
        exe_section_t section = sections[i];

        uint8_t data_pool[section.file_size];
        fread(data_pool, sizeof(uint8_t), section.file_size, f);

        printf("Section %d:\n", i + 1);
        printf("  - address     = 0x%08X\n", section.address);
        printf("  - offset      = 0x%08X\n", section.offset);
        printf("  - memory size = %d\n", section.memory_size);
        printf("  - file size   = %d\n", section.file_size);
        printf("  - align       = %d\n", section.align);
        printf("  - flags       = %c%c%c\n", section.flags & FLAG_R ? 'r' : '-', section.flags & FLAG_W ? 'w' : '-', section.flags & FLAG_X ? 'x' : '-');
        printf("\n  Instructions:\n");
        for (uint32_t j = 0; j < section.file_size; j += 4) {
            uint32_t word;
            memcpy(&word, &data_pool[j], sizeof(uint32_t));
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIANG
            word = __builtin_bswap32(word);
#endif
            
            printf("    ");
            print_instr(word);
            putchar('\n');
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s file.bin\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror(argv[1]);
        return 1;
    }

    uint32_t magic;
    fread(&magic, sizeof(uint32_t), 1, f);
    fseek(f, 0, SEEK_SET);

    if (magic == OBJ_MAGIC) {
        print_obj(f);
    } else if (magic == EXE_MAGIC) {
        print_exe(f);       
    } else {
        fprintf(stderr, "Unrecognised file format!\n");
        return 1;
    }

    return 0;
}