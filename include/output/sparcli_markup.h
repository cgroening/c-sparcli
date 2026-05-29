#pragma once

#include "core/sparcli_export.h"
#include "core/sparcli_text.h"


SPARCLI_BEGIN_DECLS

/**
 * Parser options that change how unrecognized tags are handled.
 */
typedef struct ScMarkupOpts {
    /**
     * When `true`, unrecognized tags (e.g. `[blink]`) are silently dropped
     * and only their inner content is kept; when `false` (default), the
     * tag brackets are emitted as literal text.
     */
    bool strip_unknown;
} ScMarkupOpts;


/**
 * Parses `markup` into a new `ScText` using default options.
 *
 * @param markup  Source string with Rich-style tags (e.g. `[bold red]…[/]`).
 *                Pass `NULL` to get an empty `ScText`.
 * @return        Heap-allocated `ScText`; free with `sc_text_free`.
 */
SPARCLI_EXPORT ScText *sc_markup_parse(const char *markup);

/**
 * Parses `markup` into a new `ScText` honoring `opts`.
 *
 * @param markup  Source string; may be `NULL`.
 * @param opts    Parser options (e.g. `strip_unknown`).
 * @return        Heap-allocated `ScText`; free with `sc_text_free`.
 */
SPARCLI_EXPORT ScText *sc_markup_parse_opts(
    const char *markup, ScMarkupOpts opts
);

/**
 * Parses `markup` and appends each resulting span to `text`.
 *
 * @param text    Existing `ScText` to append into.
 * @param markup  Source markup string.
 */
void sc_markup_append(ScText *text, const char *markup);

/**
 * Parses `markup` honoring `opts` and appends each span to `text`.
 *
 * @param text    Existing `ScText` to append into.
 * @param markup  Source markup string.
 * @param opts    Parser options.
 */
void sc_markup_append_opts(
    ScText *text, const char *markup, ScMarkupOpts opts
);

/**
 * Parses `markup` and prints the result to stdout (no trailing newline).
 */
void sc_markup_print(const char *markup);

/**
 * Parses `markup` honoring `opts` and prints the result (no newline).
 */
void sc_markup_print_opts(const char *markup, ScMarkupOpts opts);

/**
 * Same as `sc_markup_print` followed by `\\n`.
 */
void sc_markup_println(const char *markup);

/**
 * Same as `sc_markup_print_opts` followed by `\\n`.
 */
void sc_markup_println_opts(const char *markup, ScMarkupOpts opts);

SPARCLI_END_DECLS
