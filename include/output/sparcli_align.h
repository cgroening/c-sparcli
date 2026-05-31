#pragma once

#include "core/sparcli_rendered.h"
#include "core/sparcli_core.h"
#include "core/sparcli_text.h"

SPARCLI_BEGIN_DECLS


/**
 * Aligns each line of `rendered` within `width` columns and prints them.
 *
 * `SC_ALIGN_LEFT` is a no-op; lines are emitted as captured. For CENTER
 * and RIGHT, the spare columns (`width - line_visible_width`) are added
 * as left padding (`spare / 2` for CENTER, `spare` for RIGHT). Trailing
 * spaces are not emitted.
 *
 * @param rendered  Captured widget; no output when `NULL`.
 * @param halign     Horizontal alignment (LEFT / CENTER / RIGHT).
 * @param width     Target width in columns; `0` = current terminal width.
 */
SPARCLI_EXPORT void sc_align_print(
    const ScRendered *rendered, ScHAlign halign, int width
);

/**
 * Convenience wrapper: captures `str` and aligns it via `sc_align_print`.
 *
 * @param str    Plain string to render.
 * @param halign  Horizontal alignment.
 * @param width  Target width in columns; `0` = current terminal width.
 */
SPARCLI_EXPORT void sc_align_str(const char *str, ScHAlign halign, int width);

/**
 * Convenience wrapper: captures `text` and aligns it via `sc_align_print`.
 *
 * @param text   Rich-text object to render.
 * @param halign  Horizontal alignment.
 * @param width  Target width in columns; `0` = current terminal width.
 */
SPARCLI_EXPORT void sc_align_text(
    const ScText *text, ScHAlign halign, int width
);

SPARCLI_END_DECLS
