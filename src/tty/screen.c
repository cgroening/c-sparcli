#include "tty_internal.h"

#include <stdio.h>


static void cursor_up(int rows);


void sc_screen_draw(ScScreen *self, char *const *lines, size_t line_count) {
    // Never draw more lines than fit on screen: a frame taller than the
    // terminal would scroll and break the cursor-up arithmetic below. Cap to
    // the window height; excess lines are truncated rather than corrupting the
    // display. (Widgets bound their own height via `max_visible` etc.)
    int rows = sc_tty_rows();
    if (rows > 0 && line_count > (size_t)rows) {
        line_count = (size_t)rows;
    }

    // Rewind to the top-left of the previously drawn region and clear it.
    if (self->prev_lines > 0) {
        cursor_up(self->prev_lines - 1);
        sc_tty_puts("\r\033[0J");   // column 0, erase to end of screen
    }
    for (size_t i = 0; i < line_count; i++) {
        sc_tty_puts(lines[i] ? lines[i] : "");
        if (i + 1 < line_count) {
            sc_tty_puts("\r\n");
        }
    }
    self->prev_lines = (int)line_count;
}

void sc_screen_clear(ScScreen *self) {
    if (self->prev_lines > 0) {
        cursor_up(self->prev_lines - 1);
        sc_tty_puts("\r\033[0J");
    }
    self->prev_lines = 0;
}

/** Moves the cursor up `rows` lines; no-op when `rows <= 0`. */
static void cursor_up(int rows) {
    if (rows <= 0) {
        return;
    }
    char seq[16];
    int len = snprintf(seq, sizeof seq, "\033[%dA", rows);
    if (len > 0) {
        sc_tty_write(seq, (size_t)len);
    }
}
