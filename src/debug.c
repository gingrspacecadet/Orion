#include "debug.h"
#include "machine.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

/* Simple logger used by the debug renderer. Replace with your logging facility if desired. */
static void debug_log(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

/* Safe wrapper for vsnprintf that ensures NUL termination and returns -1 on overflow/error. */
static int safe_vsnprintf(char *buf, size_t sz, const char *fmt, va_list ap) {
    if (!buf || sz == 0 || !fmt) return -1;
    int r = vsnprintf(buf, sz, fmt, ap);
    if (r < 0) {
        if (sz) buf[0] = '\0';
        return -1;
    }
    if ((size_t)r >= sz) {
        /* truncated */
        buf[sz - 1] = '\0';
        return -1;
    }
    return r;
}

static void safe_snprintf(char *buf, size_t sz, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    (void)safe_vsnprintf(buf, sz, fmt, ap);
    va_end(ap);
}

/* Render a single line of text. Returns pixel height used (0 on error). */
static int render_text_line(SDL_Renderer* renderer, TTF_Font* font,
                            const char* text, int x, int y, SDL_Color color) {
    if (!renderer || !font || !text) return 0;

    SDL_Surface* surf = TTF_RenderText_Blended(font, text, color);
    if (!surf) {
        debug_log("TTF_RenderText_Blended failed: %s", TTF_GetError());
        return 0;
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (!tex) {
        debug_log("SDL_CreateTextureFromSurface failed: %s", SDL_GetError());
        SDL_FreeSurface(surf);
        return 0;
    }

    SDL_Rect dst = { x, y, surf->w, surf->h };
    if (SDL_RenderCopy(renderer, tex, NULL, &dst) != 0) {
        debug_log("SDL_RenderCopy failed: %s", SDL_GetError());
    }

    int h = surf->h;
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
    return h;
}

/* Return width in pixels for text. 0 on error. */
static int text_width(TTF_Font* font, const char* text) {
    if (!font || !text) return 0;
    int w = 0;
    if (TTF_SizeText(font, text, &w, NULL) != 0) {
        debug_log("TTF_SizeText failed: %s", TTF_GetError());
        return 0;
    }
    return w;
}

/* Render a memory dump region with hex + ascii. Updates *y_out to new top, returns nothing.
   center_addr is the logical address to center the view on (uint16_t as your machine uses).
   rows and bytes_per_row control vertical/horizontal layout.
   col_x is the left column X coordinate, col_width is the allowed pixel width for the region.
   Title is printed above the block. */
static void dump_memory_region_render(const Machine* m, SDL_Renderer* renderer, TTF_Font* font,
                                      uint16_t center_addr, int rows, int bytes_per_row,
                                      int col_x, int* y_out, int col_width, const char* title) {
    if (!m || !renderer || !font || !y_out || rows <= 0 || bytes_per_row <= 0 || col_width <= 0) return;

    char line[768];
    SDL_Color white = {230, 230, 230, 255};
    SDL_Color dim = {160, 160, 160, 255};
    SDL_Color pc_color = {255, 200, 80, 255};
    SDL_Color sp_color = {160, 255, 180, 255};
    int line_h = TTF_FontHeight(font);
    if (line_h <= 0) line_h = 16;

    int y = *y_out;

    /* Title with center address */
    safe_snprintf(line, sizeof(line), "%s  (0x%04X)", title ? title : "Memory", center_addr);
    y += render_text_line(renderer, font, line, col_x, y, dim);

    /* underline: draw simple dash sequence according to title pixel width */
    int title_px = text_width(font, line);
    int dash_px = text_width(font, "-");
    if (dash_px <= 0) dash_px = 6;
    int dashes = title_px / dash_px;
    if (dashes > 0) {
        size_t ulen = (size_t)dashes + 1;
        if (ulen > sizeof(line)) ulen = sizeof(line);
        for (size_t i = 0; i + 1 < ulen; ++i) line[i] = '-';
        line[ulen - 1] = '\0';
        y += render_text_line(renderer, font, line, col_x, y, dim);
    }

    /* compute addresses */
    const int total_bytes = rows * bytes_per_row;
    const int half = total_bytes / 2;
    int start = (int)center_addr - half;
    if (start < 0) start = 0;
    if (start + total_bytes > MEM_SIZE) start = MEM_SIZE - total_bytes;
    if (start < 0) start = 0;

    /* Maximum allowed text width for this column (leave small margin) */
    int max_text_px = col_width - 8;
    if (max_text_px < 50) max_text_px = 50;

    /* For each row: build hex + ascii string, mark PC/SP with color-coded inline tags. */
    for (int r = 0; r < rows; ++r) {
        int row_addr = start + r * bytes_per_row;
        /* Build hex portion and ascii portion in a temporary buffer */
        char hexpart[256];
        char asciipart[256];
        size_t ph = 0, pa = 0;
        hexpart[0] = '\0';
        asciipart[0] = '\0';

        /* Address prefix */
        int n = snprintf(line, sizeof(line), "%04X: ", row_addr);
        if (n < 0) line[0] = '\0';

        /* hex bytes */
        for (int b = 0; b < bytes_per_row; ++b) {
            int addr = row_addr + b;
            if (addr >= 0 && addr < MEM_SIZE) {
                unsigned val = (unsigned)m->mem[addr];
                n = snprintf(hexpart + ph, sizeof(hexpart) - ph, "%02X ", val);
            } else {
                n = snprintf(hexpart + ph, sizeof(hexpart) - ph, "?? ");
            }
            if (n < 0) break;
            ph += (size_t)n;
        }

        /* ascii */
        for (int b = 0; b < bytes_per_row; ++b) {
            int addr = row_addr + b;
            char ch = '.';
            if (addr >= 0 && addr < MEM_SIZE) {
                uint8_t v = m->mem[addr];
                if (v >= 32 && v <= 126) ch = (char)v;
            }
            n = snprintf(asciipart + pa, sizeof(asciipart) - pa, "%c", ch);
            if (n < 0) break;
            pa += (size_t)n;
        }

        /* Compose the full printable line (trim if needed) */
        safe_snprintf(line, sizeof(line), "%04X: %s  %s", row_addr, hexpart, asciipart);

        /* If line width exceeds allowed, attempt to shorten ascii tail first */
        int text_px = text_width(font, line);
        if (text_px > max_text_px) {
            /* We will shorten ascii part; compute a reasonable number of ascii chars to keep */
            int allowed_ratio = max_text_px * (int)strlen(line) / (text_px ? text_px : 1);
            if (allowed_ratio < 8) allowed_ratio = 8;
            size_t keep = (size_t)allowed_ratio;
            if (keep + 4 < sizeof(line)) { /* keep room for "..." and NUL */
                if (keep < strlen(line)) {
                    line[keep] = '\0';
                    strncat(line, "...", sizeof(line) - strlen(line) - 1);
                }
            }
        }

        /* Render the main line */
        render_text_line(renderer, font, line, col_x, y + r * line_h, white);

        /* Highlight individual byte positions for PC and SP on top of the line if they are in this row.
           We do this by rendering a small colored marker to the right of the hex bytes. */
        /* compute positions to draw small highlight rectangles for PC/SP */
        for (int marker = 0; marker < 2; ++marker) {
            int addr_check = (marker == 0) ? (int)m->cpu.pc : (int)m->cpu.sp;
            if (addr_check >= row_addr && addr_check < row_addr + bytes_per_row) {
                int off = addr_check - row_addr;
                /* approximate x offset by measuring text up to the byte we want */
                /* build prefix like "ADDR: " + hex bytes up to off */
                char prefix[256];
                size_t pp = 0;
                pp += snprintf(prefix + pp, sizeof(prefix) - pp, "%04X: ", row_addr);
                for (int b = 0; b <= off && pp + 4 < sizeof(prefix); ++b) {
                    int addr = row_addr + b;
                    if (addr >= 0 && addr < MEM_SIZE) {
                        pp += snprintf(prefix + pp, sizeof(prefix) - pp, "%02X ", (unsigned)m->mem[addr]);
                    } else {
                        pp += snprintf(prefix + pp, sizeof(prefix) - pp, "?? ");
                    }
                }
                int px = text_width(font, prefix);
                /* Draw a small filled rect mark just right of this byte (safe clipping) */
                SDL_Rect mark = { col_x + px - 3, y + r * line_h + 1, 6, line_h - 2 };
                SDL_Color c = (marker == 0) ? pc_color : sp_color;
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a / 2);
                SDL_RenderFillRect(renderer, &mark);
                SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
                SDL_RenderDrawRect(renderer, &mark);
            }
        }

        /* After each row continue */
    }

    /* Advance y_out */
    *y_out = y + rows * line_h + (line_h / 2);
}

/* Main improved draw_debug */
void draw_debug(Machine* m, SDL_Renderer* renderer, TTF_Font* font) {
    if (!m || !renderer || !font) {
        debug_log("draw_debug called with null parameter(s)");
        return;
    }

    /* Clear background */
    SDL_SetRenderDrawColor(renderer, 8, 12, 18, 255);
    SDL_RenderClear(renderer);

    SDL_Color white = {230, 230, 230, 255};
    SDL_Color accent = {120, 200, 255, 255};
    char buf[512];

    /* Get renderer output size */
    int win_w = 800, win_h = 600;
    if (SDL_GetRendererOutputSize(renderer, &win_w, &win_h) != 0) {
        debug_log("SDL_GetRendererOutputSize failed: %s", SDL_GetError());
    }

    /* layout metrics */
    int fh = TTF_FontHeight(font);
    if (fh <= 0) fh = 16;
    const int pad = fh;           /* main padding */
    const int gutter = fh / 2;    /* between columns */
    const int left_x = pad;
    const int right_col_x = win_w / 2 + pad;
    int y_left = pad;
    int y_right = pad;

    /* Title centered */
    safe_snprintf(buf, sizeof(buf), "CPU Debug View    [%s]    Window: %dx%d",
                  m->cpu.running ? "RUN" : "STOP", win_w, win_h);
    int tw = text_width(font, buf);
    int title_x = (win_w - tw) / 2;
    if (title_x < 0) title_x = 0;
    y_left += render_text_line(renderer, font, buf, title_x, y_left, accent);
    y_left += gutter;
    y_right = y_left;

    /* Left column: registers */
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

    /* Right column: compact status */
    int box_w = win_w / 2 - pad * 2;
    if (box_w < 160) box_w = 160;
    int box_x = win_w - pad - box_w;
    safe_snprintf(buf, sizeof(buf), "PC: 0x%04X  SP: 0x%04X  Flags: Z=%u N=%u C=%u V=%u",
                  m->cpu.pc, m->cpu.sp,
                  (unsigned)m->cpu.flags.Z, (unsigned)m->cpu.flags.N,
                  (unsigned)m->cpu.flags.C, (unsigned)m->cpu.flags.V);
    render_text_line(renderer, font, buf, box_x, y_right, accent);
    y_right += fh + gutter;

    /* Memory dump configuration */
    int bytes_per_row = (win_w > 1200) ? 16 : 8;
    int rows = (win_h - y_left - pad) / fh;
    if (rows < 6) rows = 6;
    int col_width = win_w / 2 - pad * 2;
    if (col_width < 200) col_width = 200;

    /* Left memory region centered on PC */
    int mem_y = y_left;
    dump_memory_region_render(m, renderer, font, m->cpu.pc, rows, bytes_per_row,
                              left_x, &mem_y, col_width, "Memory (PC)");

    /* Stack / SP region: place in right column if there's room, otherwise stack under memory */
    if (win_w >= 900) {
        int stack_y = y_right;
        dump_memory_region_render(m, renderer, font, m->cpu.sp, rows, bytes_per_row,
                                  right_col_x, &stack_y, col_width, "Stack (SP)");
        /* Render stack words summary under the lower of the two columns */
        int footer_y = (mem_y > stack_y) ? mem_y : stack_y;
        int info_y = footer_y + gutter;
        safe_snprintf(buf, sizeof(buf), "Stack words from SP=0x%04X:", m->cpu.sp);
        render_text_line(renderer, font, buf, left_x, info_y, accent);
        info_y += fh;
        for (int i = 0; i < 6; ++i) {
            int addr = (int)m->cpu.sp + i * 2;
            uint16_t word = 0;
            unsigned b0 = 0, b1 = 0;
            if (addr >= 0 && addr + 1 < MEM_SIZE) {
                b0 = m->mem[addr];
                b1 = m->mem[addr + 1];
                word = (uint16_t)b0 | ((uint16_t)b1 << 8);
            }
            safe_snprintf(buf, sizeof(buf), "%02d: 0x%04X   (%02X %02X)   @0x%04X",
                          i, word, b0, b1, addr);
            render_text_line(renderer, font, buf, left_x, info_y + i * fh, white);
        }
    } else {
        /* stacked layout */
        int stack_y = mem_y + gutter;
        dump_memory_region_render(m, renderer, font, m->cpu.sp, rows, bytes_per_row,
                                  left_x, &stack_y, col_width, "Stack (SP)");
        int info_y = stack_y + gutter;
        safe_snprintf(buf, sizeof(buf), "Stack words from SP=0x%04X:", m->cpu.sp);
        render_text_line(renderer, font, buf, left_x, info_y, accent);
        info_y += fh;
        for (int i = 0; i < 6; ++i) {
            int addr = (int)m->cpu.sp + i * 2;
            uint16_t word = 0;
            unsigned b0 = 0, b1 = 0;
            if (addr >= 0 && addr + 1 < MEM_SIZE) {
                b0 = m->mem[addr];
                b1 = m->mem[addr + 1];
                word = (uint16_t)b0 | ((uint16_t)b1 << 8);
            }
            safe_snprintf(buf, sizeof(buf), "%02d: 0x%04X   (%02X %02X)   @0x%04X",
                          i, word, b0, b1, addr);
            render_text_line(renderer, font, buf, left_x, info_y + i * fh, white);
        }
    }

    /* Present the frame */
    SDL_RenderPresent(renderer);
}
