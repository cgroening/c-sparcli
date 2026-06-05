#pragma once

#include "core/sparcli_export.h"

#include <stdbool.h>
#include <stddef.h>


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_process.h
 * @brief Run an external command (no shell) and capture its output.
 *
 * `sc_run` forks and `execvp`s a command described by a NULL-terminated
 * `argv` array - there is **no shell**, so arguments are passed verbatim
 * and shell metacharacters carry no special meaning (no injection surface).
 * `argv[0]` is resolved through `$PATH`. The child's stdout/stderr are
 * captured into heap buffers; optional stdin is fed without deadlocking
 * (the parent multiplexes the pipes with `poll`).
 *
 * By default the captured output is sanitized at this trust boundary
 * (ANSI escape sequences and stray control bytes removed, `\n`/`\t` kept),
 * matching the rest of the library; set `ScProcOpts.raw` to keep the bytes
 * verbatim. Output is treated as text (NUL-terminated).
 */


/**
 * Options for @ref sc_run. Zero-init = inherit environment + working
 * directory, no stdin, separate stderr, sanitized output.
 */
typedef struct ScProcOpts {
    /** Optional bytes to feed to the child's stdin; `NULL` = empty stdin. */
    const char *input;

    /** Length of `input`; `0` with a non-`NULL` `input` means `strlen`. */
    size_t input_len;

    /** Optional working directory for the child (`chdir` before exec). */
    const char *cwd;

    /** Route the child's stderr into the captured stdout (`err` stays NULL). */
    bool merge_stderr;

    /** Keep captured bytes verbatim instead of sanitizing them. */
    bool raw;
} ScProcOpts;

/**
 * Result of @ref sc_run. Free the heap buffers with
 * @ref sc_proc_result_free.
 */
typedef struct ScProcResult {
    /** `true` when the child was spawned and waited on (then `exit_code` is
        meaningful); `false` on fork/pipe setup failure. */
    bool ok;

    /** Child exit status (`WEXITSTATUS`); `127` = command not found / could
        not exec; `-1` when the child was killed by a signal. */
    int exit_code;

    /** Signal that killed the child, or `0` when it exited normally. */
    int term_signal;

    /** Captured stdout (heap, NUL-terminated); `NULL` only on failure. */
    char *out;

    /** Length of `out` in bytes (excluding the terminator). */
    size_t out_len;

    /** Captured stderr (heap, NUL-terminated); `NULL` when
        `merge_stderr` is set or on failure. */
    char *err;

    /** Length of `err` in bytes (excluding the terminator). */
    size_t err_len;
} ScProcResult;


/**
 * Runs `argv` as a child process (no shell) and captures its output.
 *
 * @param argv  NULL-terminated argument vector; `argv[0]` is the program
 *              (resolved via `$PATH`). Must have at least one element.
 * @param opts  Options (see @ref ScProcOpts); pass `(ScProcOpts){0}` for
 *              the defaults.
 * @return      An @ref ScProcResult. Always call @ref sc_proc_result_free
 *              on it (also when `ok` is `false`).
 */
SPARCLI_EXPORT ScProcResult sc_run(
    const char *const *argv, ScProcOpts opts
);

/**
 * Frees the heap buffers held by a result and clears the pointers.
 * `NULL`-safe.
 */
SPARCLI_EXPORT void sc_proc_result_free(ScProcResult *result);

SPARCLI_END_DECLS
