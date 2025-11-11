#ifndef DEBUG_H
#define DEBUG_H

#ifdef DEBUG

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Terminal helpers to read single chars and toggle noncanonical mode */
static struct termios orig_term;

static void tty_enable_raw(void) {
    struct termios t;
    tcgetattr(STDIN_FILENO, &orig_term);
    t = orig_term;
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
}

static void tty_restore(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term);
}

/* Check if there is input waiting (non-blocking) */
static int stdin_has_data(void) {
    fd_set rfds;
    struct timeval tv = {0, 0};
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    return select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv) > 0;
}

#define ANSI_RESET   "\x1b[0m"
#define ANSI_BOLD    "\x1b[1m"
#define ANSI_DIM     "\x1b[2m"
#define ANSI_RED     "\x1b[31m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_CYAN    "\x1b[36m"
#define ANSI_BG_GREY "\x1b[100m"
#define ANSI_CLEAR_SCREEN "\x1b[2J\x1b[H"

void print_cpu_state(const Machine* machine, const Machine* prev);
#endif

#endif