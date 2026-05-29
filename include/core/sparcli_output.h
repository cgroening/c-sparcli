#pragma once

#include "core/sparcli_export.h"

#include <stdio.h>


SPARCLI_BEGIN_DECLS

/**
 * Returns the `FILE *` sparcli writes its output to.
 *
 * Defaults to `stdout` when `sc_output_set_stream` has not been called (or the
 * caller has reset the output to `NULL`). All sparcli print/render
 * functions go through this stream, so redirecting it lets callers
 * capture output into a memory buffer, a log file, or a custom stream
 * without intercepting `stdout` globally.
 *
 * @return  Current output stream; never `NULL`.
 */
SPARCLI_EXPORT FILE *sc_output_stream(void);

/**
 * Sets the stream sparcli will write its output to.
 *
 * Pass `NULL` to restore the default (`stdout`). The library does not
 * take ownership of `out` — the caller remains responsible for opening,
 * flushing and closing it.
 *
 * @param out  Target stream, or `NULL` to revert to `stdout`.
 *
 * @note The output target is **thread-local**: each thread has its own, so
 * threads may render/capture concurrently to independent streams without
 * interfering. A thread that never calls this writes to `stdout`.
 */
SPARCLI_EXPORT void sc_output_set_stream(FILE *out);


/* ── Explicit-stream variants ──────────────────────────────────────────── */

/**
 * Convenience: runs `fn()` with `sc_output_set_stream(out)` active, then
 * restores the previous stream.
 *
 * Useful for scoping a block of output to a specific stream without
 * manually saving and restoring. Returns after `fn()` has run; `out` may be
 * `NULL` to render to stdout temporarily.
 *
 * @param out   Target stream; `NULL` resolves to `stdout`.
 * @param fn    Function to run with the output redirected.
 * @param ctx   Opaque pointer passed to `fn`.
 */
SPARCLI_EXPORT void sc_output_with_stream(
    FILE *out, void (*fn)(void *ctx), void *ctx
);

SPARCLI_END_DECLS
