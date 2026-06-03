#pragma once

#include "core/sparcli_rendered.h"
#include "core/sparcli_text.h"
#include "output/sparcli_table.h"

#include <stdbool.h>


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_live.h
 * @brief Live display: re-render a composed frame in place (dashboards).
 *
 * A live session repeatedly redraws a pre-rendered frame at the same
 * terminal position, so multiple widgets (tables, panels, progress bars,
 * columns, ...) can be composed into a continuously updating dashboard.
 * Frames are built with the existing capture API (`sc_capture_*`,
 * `sc_vstack`, `ScColumns`) - the live session only handles the in-place
 * redraw.
 *
 * When the output stream is not a terminal (pipe, file, capture), updates
 * are buffered and only the final frame is printed by `sc_live_end`, so
 * the same code produces clean output in scripts and CI. The
 * `SPARCLI_NO_TTY` environment override (see `sc_input_available`) also
 * forces this buffered behavior.
 *
 * @code
 * ScLive *live = sc_live_begin((ScLiveOpts){ 0 });
 * for (int i = 0; i <= 100; i++) {
 *     ScRendered *frame = build_dashboard(i);   // sc_capture_* + sc_vstack
 *     sc_live_update(live, frame);
 *     sc_rendered_free(frame);
 *     usleep(50000);
 * }
 * sc_live_end(live);   // leaves the final frame on screen, frees the handle
 * @endcode
 */


/** Opaque live session; create with `sc_live_begin`. */
typedef struct ScLive ScLive;

/**
 * Options for `sc_live_begin`. Zero-init renders in place at the current
 * cursor position with a hidden cursor and leaves the final frame on
 * screen when the session ends.
 */
typedef struct ScLiveOpts {
    /**
     * Render fullscreen on the alternate screen buffer (like `htop`); the
     * previous terminal content is restored when the session ends. The
     * final frame is then re-printed on the normal screen unless
     * `transient` is set.
     */
    bool alt_screen;

    /**
     * Keep the cursor visible during the session. Zero-init hides the
     * cursor (the natural default for live displays) and restores it on
     * `sc_live_end`.
     */
    bool show_cursor;

    /**
     * Erase the live region when the session ends instead of leaving the
     * final frame on screen. In non-terminal (buffered) sessions this
     * suppresses the final-frame output entirely.
     */
    bool transient;

    /**
     * Emit the in-place redraw escape codes even when the output stream
     * is not a terminal. Without this flag, non-terminal sessions buffer
     * updates and print only the final frame (recommended).
     */
    bool always;

    /**
     * Rows reserved below the frame for an interactive prompt. After every
     * update the cursor is parked at column 0 of the first reserved row -
     * exactly where an input widget (e.g. `sc_text_input` with
     * `hide_summary = true`) then draws and erases itself - and the next
     * update rewinds over the frame and the reserved rows together, so the
     * live display and the prompt never corrupt each other's regions.
     *
     * Reserve as many rows as the prompt actually draws (input line +
     * hint/shortcut footer lines); the frame is height-clamped so the
     * reserved rows always fit on screen and the prompt never scrolls the
     * terminal. Zero-init = 0 (classic behavior: the cursor stays at the
     * end of the frame). This is the building block for REPL dashboards;
     * see `examples/c/apps/repl_dashboard.c`.
     */
    int prompt_rows;
} ScLiveOpts;


/**
 * Starts a live session on the calling thread's `sc_output_stream()`.
 *
 * The target stream is captured at this call; later captures or stream
 * redirections on the same thread do not affect the running session.
 * Only one live session should drive a given terminal at a time.
 *
 * Cleanup safety: cursor visibility and the alternate screen are restored
 * via `atexit` on clean exits and on SIGINT/SIGTERM/SIGHUP/SIGQUIT (the
 * signal is then re-raised). Crash signals (SIGSEGV/SIGABRT) are not
 * trapped.
 *
 * @param opts  Session options; zero-init for defaults.
 * @return      Heap-allocated session handle; `NULL` only on allocation
 *              failure. Always pair with `sc_live_end`.
 */
SPARCLI_EXPORT ScLive *sc_live_begin(ScLiveOpts opts);

/**
 * Redraws the live region with `frame`.
 *
 * On a terminal the previous frame is overwritten in place (the frame is
 * clamped to the terminal height). On a non-terminal stream the frame is
 * buffered as "latest state" and nothing is printed until `sc_live_end`.
 *
 * The frame is borrowed for the duration of the call; the caller keeps
 * ownership and may free it immediately afterwards.
 *
 * @param live   Live session; `NULL` is ignored.
 * @param frame  Pre-rendered frame (`sc_capture_*` / `sc_vstack`).
 */
SPARCLI_EXPORT void sc_live_update(ScLive *live, const ScRendered *frame);

/**
 * Convenience: captures `str` and updates the live region with it.
 *
 * @param live  Live session; `NULL` is ignored.
 * @param str   Plain (multi-line) string content.
 */
SPARCLI_EXPORT void sc_live_update_str(ScLive *live, const char *str);

/**
 * Convenience: captures `text` and updates the live region with it.
 *
 * @param live  Live session; `NULL` is ignored.
 * @param text  Rich-text content.
 */
SPARCLI_EXPORT void sc_live_update_text(ScLive *live, const ScText *text);

/**
 * Convenience: renders `table` with `opts` and updates the live region.
 *
 * @param live   Live session; `NULL` is ignored.
 * @param table  Table to render.
 * @param opts   Table rendering options.
 */
SPARCLI_EXPORT void sc_live_update_table(
    ScLive *live, const ScTableData *table, ScTableOpts opts
);

/**
 * Ends the live session and frees the handle.
 *
 * Terminal sessions restore the cursor and (in `alt_screen` mode) the
 * previous screen content; the final frame stays visible unless
 * `transient` was set. Non-terminal sessions print the buffered final
 * frame once.
 *
 * @param live  Live session; `NULL`-safe. The handle is freed.
 */
SPARCLI_EXPORT void sc_live_end(ScLive *live);

SPARCLI_END_DECLS
