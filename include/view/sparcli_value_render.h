#pragma once

#include "core/sparcli_core.h"
#include "core/sparcli_text.h"
#include "core/sparcli_rendered.h"
#include "core/sparcli_export.h"
#include "serde/sparcli_value.h"


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_value_render.h
 * @brief Colored, indented pretty-printing of an `ScValue` tree (jq-style).
 *
 * Renders any `ScValue` (parsed from JSON/TOML/YAML) to the terminal with
 * syntax coloring: object keys, strings, numbers and literals each get their
 * own color. Useful for previewing structured data on the command line.
 */


/** Options for the value pretty-printer. Zero-init = 2-space indent and the
 *  default color scheme. */
typedef struct ScValueRenderOpts {
    /** Spaces of indentation per nesting level; `0` = default (2). */
    int indent;

    /** Emit object keys in ascending byte order. */
    bool sort_keys;

    /** Disable all coloring (still indented). */
    bool no_color;

    /** Object key color; zero-init = blue. */
    ScColor key_color;

    /** String value color; zero-init = green. */
    ScColor string_color;

    /** Number (int/float) color; zero-init = cyan. */
    ScColor number_color;

    /** Literal (`true`/`false`/`null`) color; zero-init = magenta. */
    ScColor literal_color;
} ScValueRenderOpts;


/**
 * Builds the colored, indented rendering of `value` as an `ScText`.
 *
 * @return  Heap `ScText` (free with `sc_text_free`); `NULL` on OOM.
 */
SPARCLI_EXPORT ScText *sc_value_render_text(
    const ScValue *value, ScValueRenderOpts opts
);

/** Prints the rendering to the current output stream (with a trailing
 *  newline). */
SPARCLI_EXPORT void sc_value_render(
    const ScValue *value, ScValueRenderOpts opts
);

/** Renders into a heap `ScRendered` (free with `sc_rendered_free`). */
SPARCLI_EXPORT ScRendered *sc_capture_value(
    const ScValue *value, ScValueRenderOpts opts
);

SPARCLI_END_DECLS
