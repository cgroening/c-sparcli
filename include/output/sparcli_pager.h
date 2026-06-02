#pragma once

#include "core/sparcli_export.h"

#include <stdbool.h>


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_pager.h
 * @brief Pager integration: route long output through `less` / `$PAGER`.
 *
 * Between `sc_pager_begin` and `sc_pager_end` everything written through
 * `sc_output_stream()` (all sparcli widgets and prints on the calling
 * thread) is piped into a pager process instead of the terminal. When the
 * output stream is not a terminal (pipe, file, capture) paging is skipped
 * and the calls become no-ops, so the same code works in scripts.
 */


/** Opaque pager session; create with `sc_pager_begin`. */
typedef struct ScPager ScPager;

/**
 * Options for `sc_pager_begin`. Zero-init pages through `$PAGER` (falling
 * back to `less -R`) and only when the output stream is a terminal.
 */
typedef struct ScPagerOpts {
    /**
     * Pager command, split on whitespace and executed without a shell;
     * `NULL`/empty = `$PAGER`, then `less -R`.
     */
    const char *command;

    /** Page even when the output stream is not a terminal. */
    bool always;
} ScPagerOpts;


/**
 * Starts a pager session: forks the pager process and redirects the
 * calling thread's `sc_output_stream()` into it.
 *
 * No-op cases (the returned handle is still valid and must be passed to
 * `sc_pager_end`): the output stream is not a terminal and `opts.always`
 * is `false`, or the pager process cannot be started.
 *
 * `SIGPIPE` is ignored for the duration of the session so that quitting
 * the pager early (e.g. `q` in `less`) does not terminate the program;
 * the previous disposition is restored by `sc_pager_end`.
 *
 * @param opts  Pager options; zero-init for defaults.
 * @return      Heap-allocated session handle; `NULL` only on allocation
 *              failure. Always pair with `sc_pager_end`.
 *
 * @code
 * ScPager *pager = sc_pager_begin((ScPagerOpts){ 0 });
 * sc_table_print(table, opts);          // paged
 * int status = sc_pager_end(pager);     // waits for the pager to exit
 * @endcode
 */
SPARCLI_EXPORT ScPager *sc_pager_begin(ScPagerOpts opts);

/**
 * Ends a pager session: flushes and closes the pipe, restores the
 * previous output stream and `SIGPIPE` disposition, waits for the pager
 * process to exit, and frees the handle.
 *
 * @param pager  Session from `sc_pager_begin`; safe to pass `NULL`.
 * @return       The pager's exit status (`0` on clean exit), `0` for
 *               no-op sessions, `-1` when the pager was killed by a
 *               signal or waiting failed.
 */
SPARCLI_EXPORT int sc_pager_end(ScPager *pager);

SPARCLI_END_DECLS
