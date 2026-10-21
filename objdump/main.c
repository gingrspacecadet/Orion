#include <stdio.h>
#include <string.h>
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
    [OP_NOP] = {OP_NOP, TYPE_X},
    [OP_LDR] = {OP_LDR, TYPE_M},
    [OP_LUI] = {OP_LUI, TYPE_A},
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

        default: {
            break;
        }
    }

    putchar('\n');
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

    obj_header_t header;
    fread(&header, sizeof(obj_header_t), 1, f);

    if (header.magic != 0x4F52004E) {
        fprintf(stderr, "Unrecognised file format!\n");
        return 1;
    }

    printf("== OrionObj v%d.%d.%d ==\n", (uint8_t)(header.version >> 24), (uint8_t)(header.version >> 16), (uint8_t)(header.version >> 8));
    printf("Sections    - %d\n", header.sections);
    printf("Symbols     - %d\n", header.syms);
    printf("Relocations - %d\n", header.relocations);
    putchar('\n');

    obj_section_t sections[header.sections];
    obj_symbol_t symbols[header.syms];
    obj_reloc_t relocs[header.relocations];

    fread(sections, sizeof(obj_section_t), header.sections, f);
    fread(symbols, sizeof(obj_symbol_t), header.syms, f);
    fread(relocs, sizeof(obj_reloc_t), header.relocations, f);

    uint8_t data_pool[header.data_pool_size];
    fread(data_pool, sizeof(uint8_t), header.data_pool_size, f);

    uint8_t string_pool[header.string_pool_size];
    fread(string_pool, sizeof(uint8_t), header.string_pool_size, f);

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
            x = __builtin_bswap32(x);
#endif
            
            printf("    ");
            print_instr(word);
        }
    }

    return 0;
}