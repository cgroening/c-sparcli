#pragma once

#include "sparcli_rendered.h"
#include "sparcli_text.h"
#include "sparcli_table.h"
#include "sparcli_list.h"
#include "sparcli_tree.h"
#include "sparcli_kv.h"
#include "sparcli_columns.h"
#include "sparcli_panel.h"
#include "sparcli_rule.h"


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
