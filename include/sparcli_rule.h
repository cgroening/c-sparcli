#pragma once

#include "sparcli_core.h"
#include "sparcli_text.h"

SPARCLI_BEGIN_DECLS


/**
* Rendering options for a horizontal rule.
*/
typedef struct ScRuleOpts {
    /** Border style; determines which `h` character is used for the line. */
    ScBorderType border_type;

    /** Line color; `SC_ANSI_COLOR_NONE` = no escape codes. */
    ScColor color;

    /**
     * Title label embedded in the line; `title.text = NULL` = no title.
     * `pos` is ignored.
     */
    ScTitle title;

    /** `0` = full terminal width; `>0` = fixed character width. */
    int width;

    /** Horizontal placement of the rule when `width > 0`. */
    ScHAlign align;

    /** Outer margin; top/bottom = blank lines, left/right = indent spaces. */
    ScEdges margin;
} ScRuleOpts;


/**
 * Renders a horizontal rule with an optional plain-string title.
 *
 * @param title  Title text embedded in the line; `NULL` = no title.
 * @param opts   Rendering options (style, color, width, alignment, margin).
 */
SPARCLI_EXPORT void sc_rule_str(const char *title, ScRuleOpts opts);

/**
 * Renders a horizontal rule with an optional rich-text title.
 *
 * @param title  Rich-text title embedded in the line; `NULL` = no title.
 * @param opts   Rendering options (style, color, width, alignment, margin).
 */
SPARCLI_EXPORT void sc_rule_text(const ScText *title, ScRuleOpts opts);

SPARCLI_END_DECLS
