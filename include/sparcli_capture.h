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

ScRendered *sc_capture_str        (const char *s);
ScRendered *sc_capture_text       (const ScText *t);
ScRendered *sc_capture_table      (const ScTableData *table_data, ScTableOpts opts);
ScRendered *sc_capture_list       (const ScList *l);
ScRendered *sc_capture_tree       (const ScTree *t);
ScRendered *sc_capture_kv         (const ScKV *kv);
ScRendered *sc_capture_columns    (const ScColumns *cl);
ScRendered *sc_capture_panel_str  (const char *content, ScPanelOpts opts);
ScRendered *sc_capture_panel_text (const ScText *content, ScPanelOpts opts);
ScRendered *sc_capture_rule_str   (const char *title, ScRuleOpts opts);
ScRendered *sc_capture_rule_text  (const ScText *title, ScRuleOpts opts);
