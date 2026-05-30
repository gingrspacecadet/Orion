#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdint.h>
#include <stddef.h>

#define MAX_SECTIONS        32
#define MAX_SECTION_SIZE    65536

typedef struct {
    char name[32];
    uint8_t buffer[MAX_SECTION_SIZE];
    uint32_t ptr;
    int id;
} Section;

extern Section sections[MAX_SECTIONS];
extern int section_count;
extern Section *active_section;

void switch_section(const char *name);

#endif