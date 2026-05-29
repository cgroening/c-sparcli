#include "tty_internal.h"

#include <stdio.h>


/* Move cursor up `n` lines (no-op for n <= 0). */
static void cursor_up(int n) {
    if (n <= 0) { return; }
    char seq[16];
    int len = snprintf(seq, sizeof seq, "\033[%dA", n);
    if (len > 0) { sc_tty_write(seq, (size_t)len); }
}

void sc_screen_draw(ScScreen *screen, char *const *lines, size_t n) {
    /* Rewind to the top-left of the previously drawn region and clear it. */
    if (screen->prev_lines > 0) {
        cursor_up(screen->prev_lines - 1);
        sc_tty_puts("\r\033[0J");   /* column 0, erase to end of screen */
    }
    for (size_t i = 0; i < n; i++) {
        sc_tty_puts(lines[i] ? lines[i] : "");
        if (i + 1 < n) { sc_tty_puts("\r\n"); }
    }
    screen->prev_lines = (int)n;
}

void sc_screen_clear(ScScreen *screen) {
    if (screen->prev_lines > 0) {
        cursor_up(screen->prev_lines - 1);
        sc_tty_puts("\r\033[0J");
    }
    screen->prev_lines = 0;
}
