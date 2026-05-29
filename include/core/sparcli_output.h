#pragma once

#include "core/sparcli_export.h"

#include <stdio.h>


SPARCLI_BEGIN_DECLS

/**
 * Returns the `FILE *` sparcli writes its output to.
 *
 * Defaults to `stdout` when `sc_set_output` has not been called (or the
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
SPARCLI_EXPORT void sc_set_output(FILE *out);


/* ── Explicit-stream variants ──────────────────────────────────────────── */

/**
 * Convenience: runs `fn()` with `sc_set_output(out)` active, then
 * restores the previous stream.
 *
 * Used to build the `_to(FILE *)` variants in a uniform way. Returns
 * after `fn()` has run; `out` may be `NULL` to render to stdout
 * temporarily.
 *
 * @param out   Target stream; `NULL` resolves to `stdout`.
 * @param fn    Function to run with the output redirected.
 * @param ctx   Opaque pointer passed to `fn`.
 */
SPARCLI_EXPORT void sc_with_output(
    FILE *out, void (*fn)(void *ctx), void *ctx
);

SPARCLI_END_DECLS
