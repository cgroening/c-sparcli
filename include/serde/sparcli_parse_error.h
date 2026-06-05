#pragma once

#include "core/sparcli_export.h"
#include "app/sparcli_error.h"

SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_parse_error.h
 * @brief `ScParseError` - the location-aware error filled in by every
 *        `sc_<format>_parse` function.
 *
 * The struct is meant to live on the caller's stack (no allocation, like the
 * `sc_args_split` error buffer): pass its address into a parser, and on
 * failure it is filled with a 1-based `line`/`column`, a human-readable
 * `message`, and a one-line `snippet` of the offending source. Render it as a
 * red panel with `sc_parse_error_to_error` + the `app/sparcli_error.h` API.
 */


/** Capacity of `ScParseError.message`, including the NUL terminator. */
#define SC_PARSE_ERROR_MESSAGE_MAX 256

/** Capacity of `ScParseError.snippet`, including the NUL terminator. */
#define SC_PARSE_ERROR_SNIPPET_MAX 128


/** Location-aware parse failure; zero-initialized when no error occurred. */
typedef struct ScParseError {
    int  line;   /**< 1-based source line; `0` when unset. */
    int  column; /**< 1-based source column; `0` when unset. */

    /** Human-readable description of what went wrong. */
    char message[SC_PARSE_ERROR_MESSAGE_MAX];

    /** Single source line around the failure (for context). */
    char snippet[SC_PARSE_ERROR_SNIPPET_MAX];
} ScParseError;


/**
 * Builds a pretty `ScError` (exit code `2`) from a parse error: the message
 * carries the location, the snippet becomes a cause line.
 *
 * @param error  Parse error to convert.
 * @return       Heap `ScError` (free with `sc_error_free` or consume with
 *               `sc_die`); `NULL` when `error` is `NULL` or on allocation
 *               failure.
 */
SPARCLI_EXPORT ScError *sc_parse_error_to_error(const ScParseError *error);

SPARCLI_END_DECLS
