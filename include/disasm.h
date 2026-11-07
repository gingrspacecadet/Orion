#ifndef DISASM_H
#define DISASM_H

#include "isa.h"

#define DISASM_IMPL
#ifndef DISASM_IMPL
extern const struct {Opcodes code; OpcodeTypes type;}
opcodes[];
extern bool disasm_fmt(uint32_t word, uint32_t pc, char *buffer, size_t buffer_size);
#else

#   define X(name, type) [OP_##name] = {OP_##name, type},
const struct {Opcodes code; OpcodeTypes type;}
opcodes[] = {
    X_OPCODES
};
#   undef X

typedef da_type(char) __disasm_fmt_cbuffer;

static bool __disasm_fmt_append(__disasm_fmt_cbuffer *buffer, char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list args2;
    va_copy(args2, args);

    int r = vsnprintf(NULL, 0, fmt, args2);
    va_end(args2);

    if (r < 0) {
        va_end(args);
        return false;
    }

    da_reserve(buffer, buffer->len + r + 1);
    vsnprintf(&buffer->data[buffer->len], r + 1, fmt, args);
    buffer->len += r;

    va_end(args);

    return true;
}

bool disasm_fmt(uint32_t word, uint32_t pc, char *buffer, size_t buffer_size) {
    __disasm_fmt_cbuffer cbuffer = {0};

    if (OPCODE(word) < OP_END) {
        if (!__disasm_fmt_append(&cbuffer, "%s ", opcode_names[OPCODE(word)]))
            return false;
    } else
        return false;

    switch (opcodes[OPCODE(word)].type) {
        case TYPE_A:
        case TYPE_M: {
            if (!__disasm_fmt_append(&cbuffer, "r%u, r%u, ", A_RD(word), A_RN(word)))
                return false;
            
            if (A_REGISTER(word)) {
                if (!__disasm_fmt_append(&cbuffer, "r%u", A_OPERAND(word) % 0xF))
                    return false;
            } else if ((int16_t)A_OPERAND(word) < 0) {
                if (!__disasm_fmt_append(&cbuffer, "#%u (%d)", A_OPERAND(word), (int16_t)A_OPERAND(word)))
                    return false;
            } else
                if (!__disasm_fmt_append(&cbuffer, "#%u", A_OPERAND(word)))
                    return false;

            break;
        }

        case TYPE_J: {
            if (!__disasm_fmt_append(&cbuffer, "#%u", B_DISPLACEMENT(word)))
                return false;
            
            if (B_DISPLACEMENT(word) & (1u << 26))
                if (!__disasm_fmt_append(&cbuffer, " (%d)", B_DISPLACEMENT(word)))
                    return false;
            break;
        }

        case TYPE_I: {
            if (!__disasm_fmt_append(&cbuffer, "r%u", I_RN(word)))
                return false;
            break;
        }

        case TYPE_S: {
            if (!__disasm_fmt_append(&cbuffer, "r%u, ", S_REG(word)))
                return false;
            
            if (S_REGISTER(word)) {
                if (!__disasm_fmt_append(&cbuffer, "r%u", S_PAYLOAD(word) & 0xF))
                    return false;
            } else {
                if (!__disasm_fmt_append(&cbuffer, "#%u", S_PAYLOAD(word)))
                    return false;

                if ((int16_t)S_PAYLOAD(word) < 0) {
                    if (!__disasm_fmt_append(&cbuffer, " (%d)", (int16_t)S_PAYLOAD(word)))
                        return false;
                }
            }

            if (S_WRITE(word)) {
                if (!__disasm_fmt_append(&cbuffer, " w"))
                    return false;
            } else {
                if (!__disasm_fmt_append(&cbuffer, " r"))
                    return false;
            }
        }

        case TYPE_X: {
            break;
        }

        default: {
            return false;
        }
    }

    if (cbuffer.len + 1 > buffer_size)
        return false;

    memcpy(buffer, cbuffer.data, cbuffer.len + 1);

    return true;
}
#endif

#endif