#include "tty_internal.h"
#include "internal.h"   /* sc_utf8_string_length (ANSI/UTF-8-aware width) */

#include <stdio.h>
#include <string.h>


static void cursor_up(int rows);
static int line_physical_rows(const char *line, int cols);


void sc_screen_draw(ScScreen *self, char *const *lines, size_t line_count) {
    // The cursor-up arithmetic works in PHYSICAL terminal rows, so a logical
    // line wider than the terminal (which the terminal soft-wraps onto several
    // rows) must be counted as the rows it actually occupies - otherwise the
    // rewind is too short and the overflow accumulates on screen.
    int rows = sc_tty_rows();
    int cols = sc_tty_cols();

    // Rewind to the top-left of the previously drawn region and clear it.
    if (self->prev_lines > 0) {
        cursor_up(self->prev_lines - 1);
        sc_tty_puts("\r\033[0J");   // column 0, erase to end of screen
    }

    // Draw lines while the running physical-row total fits the window: a frame
    // taller than the terminal would scroll and break the rewind. The first
    // line is always drawn so a single very long line still shows something.
    int drawn_rows = 0;
    for (size_t i = 0; i < line_count; i++) {
        const char *line = lines[i] ? lines[i] : "";
        int line_rows = line_physical_rows(line, cols);
        if (i > 0 && rows > 0 && drawn_rows + line_rows > rows) {
            break;
        }
        if (i > 0) {
            sc_tty_puts("\r\n");
        }
        sc_tty_puts(line);
        drawn_rows += line_rows;
    }
    // Never let prev_lines exceed the window: the cursor can't rewind above the
    // top row, so a single oversized line is capped here (it scrolls, which is
    // the documented limitation for frames taller than the terminal).
    if (rows > 0 && drawn_rows > rows) {
        drawn_rows = rows;
    }
    self->prev_lines = drawn_rows;
}

void sc_screen_clear(ScScreen *self) {
    if (self->prev_lines > 0) {
        cursor_up(self->prev_lines - 1);
        sc_tty_puts("\r\033[0J");
    }
    self->prev_lines = 0;
}

/**
 * Physical terminal rows a logical line occupies once the terminal soft-wraps
 * it at `cols`. The visible width skips ANSI escapes and counts UTF-8
 * codepoints; an empty line still occupies one row.
 */
static int line_physical_rows(const char *line, int cols) {
    if (cols <= 0) {
        return 1;
    }
    size_t width = sc_utf8_string_length(line, strlen(line));
    if (width == 0) {
        return 1;
    }
    return (int)((width + (size_t)cols - 1) / (size_t)cols);
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
