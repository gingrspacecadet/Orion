#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

static struct termios oldt;
static int termios_initialized = 0;

static void restore_termios(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldt);
}

static void init_termios(void) {
    if (termios_initialized)
        return;

    termios_initialized = 1;

    struct termios t;
    tcgetattr(STDIN_FILENO, &oldt);
    t = oldt;
    t.c_cflag &= ~(ICANON | ECHO);
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);

    atexit(restore_termios);
}

static int stdin_has_char(void) {
    init_termios();

    fd_set set;
    struct timeval tv = {0, 0};

    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);

    return select(STDIN_FILENO + 1, &set, NULL, NULL, &tv) > 0;
}

static int stdin_get_char(void) {
    init_termios();

    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) == 1)
        return c;

    return -1;
}

uint32_t uart_read(void *state, uint32_t offset, uint8_t size) {
    if (offset == 0x00) {
        if (stdin_has_char()) {
            return stdin_get_char();
        }
        return 0;
    }
    if (offset == 0x04) {
        uint32_t stat = 0;
        if (stdin_has_char()) stat |= 0x1;
        stat |= 0x2;
        return stat;
    }
    return 0;
}

void uart_write(void *state, uint32_t offset, uint32_t value, uint8_t size) {
    if (offset == 0x00) {
        putchar(value & 0xFF);
        fflush(stdout);
    }
}