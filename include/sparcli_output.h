#pragma once

#include "sparcli_export.h"

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
 * @note Not thread-safe: callers that switch streams across threads must
 * serialize the switch themselves. Once set, concurrent reads of the
 * stream pointer are safe.
 */
SPARCLI_EXPORT void sc_set_output(FILE *out);

SPARCLI_END_DECLS
