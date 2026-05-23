#pragma once

#include "sparcli_core.h"
#include "sparcli_text.h"

typedef struct {
    ScBorderType style;
    ScColor       connector_color;
    int           indent;    /* extra spaces after connector, default 1 */
    bool          no_guide;  /* suppress vertical continuation lines */
} ScTreeOpts;

typedef struct ScTreeNode ScTreeNode;
typedef struct ScTree     ScTree;

ScTree     *sc_tree_new     (ScTreeOpts opts);
ScTreeNode *sc_tree_add_str (ScTree *t, ScTreeNode *parent,
                              const char   *str,    ScTextStyle opts,
                              const char   *prefix, ScTextStyle prefix_opts);
ScTreeNode *sc_tree_add_text(ScTree *t, ScTreeNode *parent,
                              const ScText *text,
                              const char   *prefix, ScTextStyle prefix_opts);
void        sc_tree_print   (const ScTree *t);
void        sc_tree_free    (ScTree *t);
