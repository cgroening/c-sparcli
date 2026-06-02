#pragma once

#include "sparcli.h"

#include <stddef.h>


/**
 * @file text_internal.h
 * @brief Private struct layout for `ScText`.
 *
 * The public API treats `ScText` as opaque (only the forward declaration
 * is visible in `sparcli_text.h`). sparcli's own source files include
 * this header to access the fields directly and avoid accessor-call
 * overhead in the hot rendering paths. External consumers use the
 * accessor functions in `sparcli_text.h`.
 */

struct ScText {
    ScSpan *spans;
    size_t  count;
    size_t  capacity;
};


/**
 * Appends a span WITHOUT sanitizing `raw_str`.
 *
 * Internal trusted path for strings that are already sanitized (widgets
 * that resolved their own `ScAnsiMode`) or that are library-generated.
 * The public `sc_text_append` sanitizes per the global setting and then
 * delegates here.
 */
void sc_text_append_raw(
    ScText *sc_text, const char *raw_str, ScTextStyle style
);
