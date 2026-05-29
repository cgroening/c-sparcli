#pragma once

#include "core/sparcli_rendered.h"
#include "core/sparcli_text.h"
#include "output/sparcli_table.h"
#include "output/sparcli_list.h"
#include "output/sparcli_tree.h"
#include "output/sparcli_kv.h"
#include "output/sparcli_columns.h"
#include "output/sparcli_panel.h"
#include "output/sparcli_rule.h"

SPARCLI_BEGIN_DECLS


/**
 * Captures a plain string as a heap-allocated `ScRendered`.
 *
 * All `sc_capture_*` functions render the widget into an off-screen
 * buffer, split the output on newlines, and return an `ScRendered` whose
 * `lines` retain their original ANSI codes. The caller must free the
 * result with `sc_rendered_free`.
 *
 * @param str  Source string; a trailing newline is added when missing.
 * @return     Heap-allocated `ScRendered`; free with `sc_rendered_free`.
 */
ScRendered *sc_capture_str(const char *str);

/** Captures a rich-text object. See `sc_capture_str`. */
ScRendered *sc_capture_text(const ScText *text);

/** Captures a table rendering. See `sc_capture_str`. */
ScRendered *sc_capture_table(const ScTableData *table, ScTableOpts opts);

/** Captures a list rendering. See `sc_capture_str`. */
ScRendered *sc_capture_list(const ScList *list);

/** Captures a tree rendering. See `sc_capture_str`. */
ScRendered *sc_capture_tree(const ScTree *tree);

/** Captures a key-value rendering. See `sc_capture_str`. */
ScRendered *sc_capture_kv(const ScKV *kv);

/** Captures a columns rendering. See `sc_capture_str`. */
ScRendered *sc_capture_columns(const ScColumns *columns);

/** Captures a plain-string panel rendering. See `sc_capture_str`. */
ScRendered *sc_capture_panel_str(const char *content, ScPanelOpts opts);

/** Captures a rich-text panel rendering. See `sc_capture_str`. */
ScRendered *sc_capture_panel_text(const ScText *content, ScPanelOpts opts);

/** Captures a horizontal rule with a plain-string title. */
ScRendered *sc_capture_rule_str(const char *title, ScRuleOpts opts);

/** Captures a horizontal rule with a rich-text title. */
ScRendered *sc_capture_rule_text(const ScText *title, ScRuleOpts opts);

/**
 * Stacks captured renderings vertically into a single `ScRendered`.
 *
 * Lines from `parts[0]` come first, then `gap` blank lines, then `parts[1]`,
 * and so on. The result's `max_column_width` is the widest line across all
 * parts. This is the building block for placing two (or more) widgets one
 * above the other inside a single `sc_columns_add_rendered` column.
 *
 * The inputs are **not** consumed: the caller still owns every `parts[i]`
 * and must free them (and the returned value) with `sc_rendered_free`.
 *
 * @param parts  Array of `n` non-NULL `ScRendered` pointers, top to bottom.
 * @param n      Number of parts; returns NULL when 0.
 * @param gap    Blank lines inserted between adjacent parts (clamped to >= 0).
 * @return       Heap-allocated `ScRendered`; free with `sc_rendered_free`.
 */
ScRendered *sc_vstack(const ScRendered *const *parts, size_t n, int gap);

SPARCLI_END_DECLS
