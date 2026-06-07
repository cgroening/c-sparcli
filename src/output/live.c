#include "sparcli.h"
#include "internal.h"   /* sc_terminal_height */

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* ── Terminal escape sequences ──────────────────────────────────────────── */

#define LIVE_CURSOR_HIDE      "\033[?25l"
#define LIVE_CURSOR_SHOW      "\033[?25h"
#define LIVE_ALT_SCREEN_ENTER "\033[?1049h\033[H"
#define LIVE_ALT_SCREEN_LEAVE "\033[?1049l"
#define LIVE_ERASE_LINE_END   "\033[K"
#define LIVE_ERASE_BELOW      "\033[J"


/** Live session state (opaque to callers). */
struct ScLive {
    FILE *stream;        /**< Output stream captured at begin. */
    ScLiveOpts opts;

    /** `true` = redraw escape codes are emitted (terminal or `always`). */
    bool emit;

    /** `true` = the latest frame is kept for the final print on end. */
    bool store_frames;

    /** Total rows drawn by the previous update (frame + pads + reserve). */
    int prev_lines;

    /** Rows from the parked cursor up to the region top (the cursor parks at
        the first reserved row, where the prompt draws downward). */
    int prev_up;

    /** Newline-joined copy of the latest frame; printed by `sc_live_end`. */
    char *last_frame;
};


/*
 * Cleanup safety: one process-global record of the terminal state to restore
 * if the program exits (atexit) or is interrupted (SIGINT/TERM/HUP/QUIT)
 * while a live session is active. Only async-signal-safe calls are used in
 * the handler (`write`, `sigaction`, `raise`). Crash signals are deliberately
 * not trapped (same stance as the tty layer: keep debugger/sanitizer
 * handlers intact).
 */
static const int CLEANUP_SIGNALS[] = { SIGINT, SIGTERM, SIGHUP, SIGQUIT };
#define N_CLEANUP_SIGNALS \
    ((int)(sizeof CLEANUP_SIGNALS / sizeof CLEANUP_SIGNALS[0]))

static volatile sig_atomic_t cleanup_fd = -1;        /* fd to restore */
static volatile sig_atomic_t cleanup_show_cursor = 0;
static volatile sig_atomic_t cleanup_leave_alt = 0;
static bool cleanup_atexit_registered = false;
static bool cleanup_handlers_installed = false;
static struct sigaction cleanup_old_actions[N_CLEANUP_SIGNALS];


/* ── Internal helpers (call order) ──────────────────────────────────────── */

static void begin_terminal_modes(ScLive *live);
static void draw_frame(ScLive *live, const ScRendered *frame);
static void store_last_frame(ScLive *live, const ScRendered *frame);
static void end_emitting_session(ScLive *live);
    static void erase_region(ScLive *live);
static void end_buffered_session(ScLive *live);

static void cleanup_register(const ScLive *live);
    static void cleanup_install_handlers(void);
        static void cleanup_on_signal(int signum);
            static void cleanup_restore_terminal(void);
    static void cleanup_on_exit(void);
static void cleanup_unregister(void);


ScLive *sc_live_begin(ScLiveOpts opts) {
    ScLive *live = calloc(1, sizeof(ScLive));
    if (!live) {
        return NULL;
    }
    live->opts = opts;
    live->stream = sc_output_stream();

    // Off-terminal output (pipe, file, capture) buffers updates and prints
    // only the final frame, unless `always` forces escape emission. The
    // SPARCLI_NO_TTY override counts as "off terminal" too.
    int fd = fileno(live->stream);
    bool is_terminal = !sc_no_tty_override() && fd >= 0 && isatty(fd);
    live->emit = is_terminal || opts.always;

    // The latest frame is kept whenever the end of the session needs to
    // print it: buffered sessions always, alt-screen sessions to re-print
    // the final state on the normal screen.
    live->store_frames = !opts.transient && (!live->emit || opts.alt_screen);

    if (live->emit) {
        begin_terminal_modes(live);
    }
    if (is_terminal) {
        cleanup_register(live);
    }
    return live;
}

void sc_live_update(ScLive *live, const ScRendered *frame) {
    if (!live || !frame) {
        return;
    }
    if (live->store_frames) {
        store_last_frame(live, frame);
    }
    if (live->emit) {
        draw_frame(live, frame);
    }
}

void sc_live_update_str(ScLive *live, const char *str) {
    if (!live || !str) {
        return;
    }
    ScRendered *frame = sc_capture_str(str);
    if (frame) {
        sc_live_update(live, frame);
        sc_rendered_free(frame);
    }
}

void sc_live_update_text(ScLive *live, const ScText *text) {
    if (!live || !text) {
        return;
    }
    ScRendered *frame = sc_capture_text(text);
    if (frame) {
        sc_live_update(live, frame);
        sc_rendered_free(frame);
    }
}

void sc_live_update_table(ScLive *live, const ScTableData *table,
                          ScTableOpts opts) {
    if (!live || !table) {
        return;
    }
    ScRendered *frame = sc_capture_table(table, opts);
    if (frame) {
        sc_live_update(live, frame);
        sc_rendered_free(frame);
    }
}

void sc_live_end(ScLive *live) {
    if (!live) {
        return;
    }
    if (live->emit) {
        end_emitting_session(live);
    } else {
        end_buffered_session(live);
    }
    cleanup_unregister();
    fflush(live->stream);
    free(live->last_frame);
    free(live);
}

/** Enters the terminal modes requested by the opts (alt screen, cursor). */
static void begin_terminal_modes(ScLive *live) {
    if (live->opts.alt_screen) {
        fputs(LIVE_ALT_SCREEN_ENTER, live->stream);
    }
    if (!live->opts.show_cursor) {
        fputs(LIVE_CURSOR_HIDE, live->stream);
    }
    fflush(live->stream);
}

/**
 * Redraws the live region in place: rewinds to the first line of the
 * previous frame, overwrites line by line (erasing stale content to the
 * right), and erases leftover lines from a previously taller frame.
 *
 * With `prompt_rows` reserved, the cursor is then parked at column 0 of the
 * first reserved row (where an interactive prompt runs); the reserved rows
 * count into `prev_lines`, so the next rewind still lands on the frame top.
 */
static void draw_frame(ScLive *live, const ScRendered *frame) {
    FILE *out = live->stream;
    int reserve = live->opts.prompt_rows > 0 ? live->opts.prompt_rows : 0;

    // Never draw more lines than the terminal has rows: a taller frame
    // would scroll the screen and break the cursor-up arithmetic. The
    // reserved prompt rows always survive the clamp; the frame shrinks.
    size_t count = frame->line_count;
    int rows = sc_terminal_height();
    if (rows > 0 && reserve >= rows) {
        reserve = rows - 1;
    }
    if (rows > 0 && count + (size_t)reserve > (size_t)rows) {
        count = (size_t)(rows - reserve);
    }

    // Vertical alignment (alt-screen only): pad with blank rows so the block
    // (frame + reserve) - or just the reserve, with valign_fixed_header - sits
    // top/middle/bottom of the screen. The pads are part of the drawn region,
    // so the cursor-up rewind and the prompt park row stay consistent.
    int top_pad = 0, mid_gap = 0;
    if (live->opts.alt_screen && live->opts.valign != SC_VALIGN_TOP
        && rows > 0) {
        if (live->opts.valign_fixed_header) {
            int free_rows = rows - (int)count - reserve;
            if (free_rows < 0) { free_rows = 0; }
            mid_gap = live->opts.valign == SC_VALIGN_MIDDLE
                ? free_rows / 2 : free_rows;
        } else {
            int free_rows = rows - (int)count - reserve;
            if (free_rows < 0) { free_rows = 0; }
            top_pad = live->opts.valign == SC_VALIGN_MIDDLE
                ? free_rows / 2 : free_rows;
        }
    }

    // Rewind from the parked cursor (the first reserved row) to the region top.
    if (live->prev_up > 0) {
        fprintf(out, "\033[%dA", live->prev_up);
    }
    if (live->prev_lines > 0) {
        fputc('\r', out);
    }

    // Leading blank rows (whole-block alignment).
    for (int i = 0; i < top_pad; i++) {
        fputs(LIVE_ERASE_LINE_END, out);
        fputc('\n', out);
    }

    for (size_t i = 0; i < count; i++) {
        fputs(frame->lines[i] ? frame->lines[i] : "", out);
        fputs(LIVE_ERASE_LINE_END, out);
        if (i + 1 < count) {
            fputc('\n', out);
        }
    }

    // Gap rows between a fixed header and the aligned widget region.
    for (int i = 0; i < mid_gap; i++) {
        fputc('\n', out);
        fputs(LIVE_ERASE_LINE_END, out);
    }
    fputs(LIVE_ERASE_BELOW, out);   // clear the reserve area (below the frame)

    // Park at the FIRST reserved row so the prompt draws downward into the
    // reserve (one newline past the frame); the prompt owns the rest of it.
    int up_to_top = top_pad + (int)count + mid_gap;
    if (reserve > 0) {
        fputc('\n', out);
        fputc('\r', out);
        live->prev_up = up_to_top;            /* park = first reserved row */
    } else {
        live->prev_up = up_to_top - 1;        /* park = last frame line */
    }

    live->prev_lines = top_pad + (int)count + mid_gap + reserve;
    fflush(out);
}

/**
 * Keeps a newline-joined heap copy of `frame` as the latest state. On
 * allocation failure the previously stored frame is kept.
 */
static void store_last_frame(ScLive *live, const ScRendered *frame) {
    size_t total = 1;
    for (size_t i = 0; i < frame->line_count; i++) {
        total += (frame->lines[i] ? strlen(frame->lines[i]) : 0) + 1;
    }
    char *joined = malloc(total);
    if (!joined) {
        return;
    }
    size_t pos = 0;
    for (size_t i = 0; i < frame->line_count; i++) {
        const char *line = frame->lines[i] ? frame->lines[i] : "";
        size_t len = strlen(line);
        memcpy(joined + pos, line, len);
        pos += len;
        joined[pos++] = '\n';
    }
    joined[pos] = '\0';

    free(live->last_frame);
    live->last_frame = joined;
}

/**
 * Ends a session that emitted escape codes: restores the cursor and the
 * normal screen, leaving the final frame visible unless `transient`.
 */
static void end_emitting_session(ScLive *live) {
    FILE *out = live->stream;

    if (live->opts.alt_screen) {
        // Leaving the alternate screen discards everything drawn there and
        // restores the previous terminal content.
        fputs(LIVE_ALT_SCREEN_LEAVE, out);
        if (!live->opts.transient && live->last_frame) {
            fputs(live->last_frame, out);
        }
    } else if (live->opts.transient) {
        erase_region(live);
    } else if (live->prev_lines > 0) {
        // Leave the final frame in place; subsequent output starts below it.
        fputc('\n', out);
    }

    if (!live->opts.show_cursor) {
        fputs(LIVE_CURSOR_SHOW, out);
    }
}

/** Erases the in-place live region (transient end). */
static void erase_region(ScLive *live) {
    FILE *out = live->stream;
    if (live->prev_lines <= 0) {
        return;
    }
    if (live->prev_up > 0) {   /* up from the park to the region top */
        fprintf(out, "\033[%dA", live->prev_up);
    }
    fputc('\r', out);
    fputs(LIVE_ERASE_BELOW, out);
    live->prev_lines = 0;
    live->prev_up = 0;
}

/** Ends a buffered (non-terminal) session: prints the final frame once. */
static void end_buffered_session(ScLive *live) {
    if (!live->opts.transient && live->last_frame) {
        fputs(live->last_frame, live->stream);
    }
}


/* ── Cleanup safety (atexit + interrupt signals) ────────────────────────── */

/** Records the terminal state to restore and installs the cleanup hooks. */
static void cleanup_register(const ScLive *live) {
    int fd = fileno(live->stream);
    if (fd < 0) {
        return;
    }
    bool hides_cursor = !live->opts.show_cursor;
    bool uses_alt_screen = live->opts.alt_screen;
    if (!hides_cursor && !uses_alt_screen) {
        return;   // nothing to restore on abnormal exit
    }

    cleanup_fd = fd;
    cleanup_show_cursor = hides_cursor;
    cleanup_leave_alt = uses_alt_screen;

    if (!cleanup_atexit_registered) {
        atexit(cleanup_on_exit);
        cleanup_atexit_registered = true;
    }
    cleanup_install_handlers();
}

/** Installs the interrupt-signal handlers, saving the previous ones. */
static void cleanup_install_handlers(void) {
    if (cleanup_handlers_installed) {
        return;
    }
    struct sigaction action;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    action.sa_handler = cleanup_on_signal;
    for (int i = 0; i < N_CLEANUP_SIGNALS; i++) {
        sigaction(CLEANUP_SIGNALS[i], &action, &cleanup_old_actions[i]);
    }
    cleanup_handlers_installed = true;
}

/** Restores the terminal, then re-raises `signum` with the default handler. */
static void cleanup_on_signal(int signum) {
    cleanup_restore_terminal();
    struct sigaction dfl;
    sigemptyset(&dfl.sa_mask);
    dfl.sa_flags = 0;
    dfl.sa_handler = SIG_DFL;
    sigaction(signum, &dfl, NULL);
    raise(signum);
}

/**
 * Restores cursor visibility and leaves the alternate screen. Idempotent
 * and async-signal-safe (raw `write` only), so it is callable from a
 * signal handler.
 */
static void cleanup_restore_terminal(void) {
    int fd = cleanup_fd;
    if (fd < 0) {
        return;
    }
    ssize_t ignored = 0;
    if (cleanup_leave_alt) {
        ignored = write(fd, LIVE_ALT_SCREEN_LEAVE,
                        sizeof(LIVE_ALT_SCREEN_LEAVE) - 1);
    }
    if (cleanup_show_cursor) {
        ignored = write(fd, LIVE_CURSOR_SHOW, sizeof(LIVE_CURSOR_SHOW) - 1);
    }
    (void)ignored;
    cleanup_fd = -1;
    cleanup_show_cursor = 0;
    cleanup_leave_alt = 0;
}

/** `atexit` hook: guarantees restoration if the caller exits mid-session. */
static void cleanup_on_exit(void) {
    cleanup_restore_terminal();
}

/** Clears the cleanup record and restores the saved signal handlers. */
static void cleanup_unregister(void) {
    cleanup_fd = -1;
    cleanup_show_cursor = 0;
    cleanup_leave_alt = 0;
    if (cleanup_handlers_installed) {
        for (int i = 0; i < N_CLEANUP_SIGNALS; i++) {
            sigaction(CLEANUP_SIGNALS[i], &cleanup_old_actions[i], NULL);
        }
        cleanup_handlers_installed = false;
    }
}
