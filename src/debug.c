#include "debug.h"
#include "machine.h"

static int render_text_line(SDL_Renderer* renderer, TTF_Font* font,
                            const char* text, int x, int y, SDL_Color color) {
    if (!renderer || !font || !text) return 0;
    SDL_Surface* surf = TTF_RenderText_Blended(font, text, color);
    if (!surf) return 0;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
        SDL_Rect dst = { x, y, surf->w, surf->h };
        SDL_RenderCopy(renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    int h = surf->h;
    SDL_FreeSurface(surf);
    return h;
}

static int text_width(TTF_Font* font, const char* text) {
    if (!font || !text) return 0;
    int w = 0;
    TTF_SizeText(font, text, &w, NULL);
    return w;
}

static void safe_snprintf(char* buf, size_t sz, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sz, fmt, ap);
    va_end(ap);
}

// Render a memory dump block (hex + ascii) at (col_x, *y_out).
// Returns height used (also updates *y_out to new top).
static void dump_memory_region_render(const Machine* m, SDL_Renderer* renderer, TTF_Font* font,
                                      uint16_t center_addr, int rows, int bytes_per_row,
                                      int col_x, int* y_out, int col_width, const char* title) {
    char line[512];
    SDL_Color white = {255,255,255,255};
    int line_h = TTF_FontHeight(font);
    int y = *y_out;

    // Title (bold look by adding underline dash)
    safe_snprintf(line, sizeof(line), "%s  (0x%04X)", title, center_addr);
    y += render_text_line(renderer, font, line, col_x, y, white);
    // underline
    int tw = text_width(font, line);
    char underline[128];
    int dashes = tw / (text_width(font, "-") ?: 1);
    if (dashes > 0 && dashes < (int)sizeof(underline)-1) {
        for (int i = 0; i < dashes; ++i) underline[i] = '-';
        underline[dashes] = '\0';
        y += render_text_line(renderer, font, underline, col_x, y, white);
    }

    // Compute start address centered on center_addr
    int total_bytes = rows * bytes_per_row;
    int half = total_bytes / 2;
    int start = (int)center_addr - half;
    if (start < 0) start = 0;
    if (start + total_bytes > MEM_SIZE) start = MEM_SIZE - total_bytes;
    if (start < 0) start = 0;

    // For each row print: "ADDR: XX XX ...  ASCII..."
    for (int r = 0; r < rows; ++r) {
        int row_addr = start + r * bytes_per_row;
        int p = 0;
        p += snprintf(line + p, sizeof(line) - p, "%04X: ", row_addr);
        // hex bytes (pad single digits)
        for (int b = 0; b < bytes_per_row; ++b) {
            int addr = row_addr + b;
            if (addr >= 0 && addr < MEM_SIZE) {
                p += snprintf(line + p, sizeof(line) - p, "%02X ", m->mem[addr]);
            } else {
                p += snprintf(line + p, sizeof(line) - p, "?? ");
            }
        }
        // gap then ascii
        p += snprintf(line + p, sizeof(line) - p, "  ");
        for (int b = 0; b < bytes_per_row; ++b) {
            int addr = row_addr + b;
            char ch = '.';
            if (addr >= 0 && addr < MEM_SIZE) {
                uint8_t v = m->mem[addr];
                if (v >= 32 && v <= 126) ch = (char)v;
            }
            p += snprintf(line + p, sizeof(line) - p, "%c", ch);
        }
        // markers for PC/SP
        if ((int)m->cpu.pc >= row_addr && (int)m->cpu.pc < row_addr + bytes_per_row) {
            int off = m->cpu.pc - row_addr;
            p += snprintf(line + p, sizeof(line) - p, "   <PC@%02X>", off);
        }
        if ((int)m->cpu.sp >= row_addr && (int)m->cpu.sp < row_addr + bytes_per_row) {
            int off = m->cpu.sp - row_addr;
            p += snprintf(line + p, sizeof(line) - p, "   <SP@%02X>", off);
        }

        // Clip line if it exceeds column width (nice for small windows)
        int max_w = col_width - 8;
        int text_w = text_width(font, line);
        if (text_w > max_w) {
            // naive trim: shorten ascii tail
            int keep = (int)(strlen(line) * (double)max_w / (double)text_w);
            if (keep > 3) {
                line[keep-3] = '.'; line[keep-2] = '.'; line[keep-1] = '.'; line[keep] = '\0';
            }
        }

        render_text_line(renderer, font, line, col_x, y + r * line_h, white);
    }

    *y_out = y + rows * line_h + line_h/2;
}

// Main improved draw_debug
void draw_debug(Machine* m, SDL_Renderer* renderer, TTF_Font* font) {
    if (!m || !renderer || !font) return;

    // Clear background
    SDL_SetRenderDrawColor(renderer, 8, 12, 18, 255); // dark slightly bluish background
    SDL_RenderClear(renderer);

    SDL_Color white = {230, 230, 230, 255};
    SDL_Color accent = {120, 200, 255, 255};
    char buf[512];

    // Get renderer output size
    int win_w = 800, win_h = 600;
    SDL_GetRendererOutputSize(renderer, &win_w, &win_h);

    // Font metrics and layout constants
    int fh = TTF_FontHeight(font);
    if (fh <= 0) fh = 16;
    const int pad = fh;           // main padding
    const int gutter = fh / 2;    // between columns
    const int left_x = pad;
    const int right_x = win_w / 2 + pad;
    int y_left = pad;
    int y_right = pad;

    // Title (center top)
    safe_snprintf(buf, sizeof(buf), "CPU Debug View    [%s]    Window: %dx%d",
                  m->cpu.running ? "RUN" : "STOP", win_w, win_h);
    int tw = text_width(font, buf);
    int title_x = (win_w - tw) / 2;
    render_text_line(renderer, font, buf, title_x, y_left, accent);
    y_left += fh + gutter;
    y_right = y_left;

    // Left column: registers, PC/SP, flags
    for (int i = 0; i < 8; ++i) {
        safe_snprintf(buf, sizeof(buf), "R%-2d = 0x%02X   (%3u)", i, m->cpu.reg[i], m->cpu.reg[i]);
        render_text_line(renderer, font, buf, left_x, y_left + i * fh, white);
    }
    y_left += 8 * fh + gutter;

    safe_snprintf(buf, sizeof(buf), "PC = 0x%04X    SP = 0x%04X", m->cpu.pc, m->cpu.sp);
    render_text_line(renderer, font, buf, left_x, y_left, white);
    y_left += fh;

    safe_snprintf(buf, sizeof(buf), "Flags: Z=%u  N=%u  C=%u  V=%u",
                  (unsigned)m->cpu.flags.Z, (unsigned)m->cpu.flags.N,
                  (unsigned)m->cpu.flags.C, (unsigned)m->cpu.flags.V);
    render_text_line(renderer, font, buf, left_x, y_left, white);
    y_left += fh + gutter;

    // Right column: summary boxes aligned to right edge (nice alignment)
    int box_w = win_w / 2 - pad*2;
    int box_x = win_w - pad - box_w;
    // Small status box (top-right)
    safe_snprintf(buf, sizeof(buf), "PC: 0x%04X  SP: 0x%04X  Flags: Z=%u N=%u C=%u V=%u",
                  m->cpu.pc, m->cpu.sp,
                  (unsigned)m->cpu.flags.Z, (unsigned)m->cpu.flags.N,
                  (unsigned)m->cpu.flags.C, (unsigned)m->cpu.flags.V);
    render_text_line(renderer, font, buf, box_x, y_right, accent);
    y_right += fh + gutter;

    // Memory dumps: try side-by-side if width permits, else stack
    int bytes_per_row = (win_w > 1200) ? 16 : 8;
    int rows = (win_h - y_left - pad) / fh;
    if (rows < 6) rows = 6;
    int col_width = win_w / 2 - pad*2;

    // Left memory block (below left column)
    int mem_y = y_left;
    dump_memory_region_render(m, renderer, font, m->cpu.pc, rows, bytes_per_row,
                              left_x, &mem_y, col_width, "Memory (PC)");

    // Stack block: place top-aligned at right column if enough width, otherwise stack under memory block
    if (win_w >= 900) {
        int stack_y = y_right;
        dump_memory_region_render(m, renderer, font, m->cpu.sp, rows, bytes_per_row,
                                  win_w/2 + pad, &stack_y, col_width, "Stack (SP)");
        // set footer y to max of both
        int footer_y = (mem_y > stack_y) ? mem_y : stack_y;
        // Stack detail under both
        int info_y = footer_y + gutter;
        safe_snprintf(buf, sizeof(buf), "Stack words from SP=0x%04X:", m->cpu.sp);
        render_text_line(renderer, font, buf, left_x, info_y, accent);
        info_y += fh;
        for (int i = 0; i < 6; ++i) {
            int addr = (int)m->cpu.sp + i*2;
            uint16_t word = 0;
            if (addr >= 0 && addr + 1 < MEM_SIZE) {
                word = (uint16_t)m->mem[addr] | ((uint16_t)m->mem[addr+1] << 8);
            }
            safe_snprintf(buf, sizeof(buf), "%02d: 0x%04X   (%02X %02X)   @0x%04X",
                          i, word,
                          (addr >= 0 && addr < MEM_SIZE) ? m->mem[addr] : 0x00,
                          (addr+1 >= 0 && addr+1 < MEM_SIZE) ? m->mem[addr+1] : 0x00,
                          addr);
            render_text_line(renderer, font, buf, left_x, info_y + i * fh, white);
        }
    } else {
        // stacked layout: stack dump below memory dump
        int stack_y = mem_y + gutter;
        dump_memory_region_render(m, renderer, font, m->cpu.sp, rows, bytes_per_row,
                                  left_x, &stack_y, col_width, "Stack (SP)");
        int info_y = stack_y + gutter;
        safe_snprintf(buf, sizeof(buf), "Stack words from SP=0x%04X:", m->cpu.sp);
        render_text_line(renderer, font, buf, left_x, info_y, accent);
        info_y += fh;
        for (int i = 0; i < 6; ++i) {
            int addr = (int)m->cpu.sp + i*2;
            uint16_t word = 0;
            if (addr >= 0 && addr + 1 < MEM_SIZE) {
                word = (uint16_t)m->mem[addr] | ((uint16_t)m->mem[addr+1] << 8);
            }
            safe_snprintf(buf, sizeof(buf), "%02d: 0x%04X   (%02X %02X)   @0x%04X",
                          i, word,
                          (addr >= 0 && addr < MEM_SIZE) ? m->mem[addr] : 0x00,
                          (addr+1 >= 0 && addr+1 < MEM_SIZE) ? m->mem[addr+1] : 0x00,
                          addr);
            render_text_line(renderer, font, buf, left_x, info_y + i * fh, white);
        }
    }

    SDL_RenderPresent(renderer);
}