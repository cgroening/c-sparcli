#pragma once

#include "input/sparcli_term.h"

#include <stddef.h>
#include <stdbool.h>


/**
 * @file tty_internal.h
 * @brief Private terminal-I/O layer shared by `src/tty/` and `src/input/`.
 *
 * Everything here talks to the controlling terminal directly (a `/dev/tty`
 * file descriptor), never through the redirectable `sc_output_stream()`.
 * That is the hard boundary between sparcli's output side (stream-oriented)
 * and its input side (tty-oriented).
 */


/* ── Raw mode / terminal lifecycle (term.c) ─────────────────────────────── */

/**
 * Enters raw mode on the controlling terminal and hides the cursor.
 *
 * Opens `/dev/tty` (falling back to stdin/stdout), saves the current
 * `termios`, disables canonical mode, echo and signal generation (so
 * Ctrl-C arrives as a byte rather than a signal), and installs cleanup
 * handlers so the terminal is always restored on exit or fatal signal.
 *
 * @return `SC_INPUT_OK` on success, `SC_INPUT_ERROR` when no TTY is usable.
 */
ScInputStatus sc_tty_begin(void);

/** Restores the saved terminal mode, shows the cursor, releases the fd. */
void sc_tty_end(void);

/** Writes `len` bytes to the terminal output fd. */
void sc_tty_write(const char *bytes, size_t len);

/** Writes a NUL-terminated string to the terminal output fd. */
void sc_tty_puts(const char *str);

/** Returns the descriptor used for terminal reads (shared with key.c). */
int sc_tty_internal_fd(void);

/** Returns the terminal height in rows (fallback 24 when unknown). */
int sc_tty_rows(void);

/** Returns and clears the pending-resize flag set by the SIGWINCH handler. */
bool sc_tty_take_resize(void);


/* ── Key input (key.c) ──────────────────────────────────────────────────── */

/**
 * Blocks until a key is available on the terminal input fd and returns it
 * decoded. Returns a `SC_KEY_NONE` key on EOF/read error, or a `SC_KEY_RESIZE`
 * key when interrupted by a terminal resize.
 *
 * Unrecognized escape sequences and stray control bytes (e.g. pasted ANSI
 * color codes) are skipped silently - they are never returned as keys and
 * never inserted as text, so `SC_KEY_NONE` from this function always means
 * EOF or a read error.
 */
ScKey sc_tty_read_key(void);

/**
 * Discards any buffered, not-yet-decoded input bytes (called at session
 * start so stale bytes never leak between prompts).
 */
void sc_tty_input_reset(void);


/* ── In-place multi-line redraw (screen.c) ──────────────────────────────── */

/**
 * State for a multi-line in-place redraw region. Zero-initialize before the
 * first draw (`(ScScreen){0}`).
 */
typedef struct ScScreen {
    /** Number of lines drawn by the previous `sc_screen_draw`. */
    int prev_lines;
} ScScreen;

/**
 * Redraws the region in place: moves up over the previously drawn lines,
 * erases downward, then writes `line_count` lines separated by CR-LF (no
 * trailing newline; the cursor ends on the last line).
 */
void sc_screen_draw(ScScreen *self, char *const *lines, size_t line_count);

/**
 * Erases the drawn region and parks the cursor at its top-left, so the
 * caller can print a persistent summary line in its place.
 */
void sc_screen_clear(ScScreen *self);
