#pragma once

#include "sparcli_types.h"
#include "sparcli_text.h"

typedef struct {
    ScBorderStyle style;
    ScColor       connector_color;
    int           indent;    /* extra spaces after connector, default 1 */
    int           no_guide;  /* 1 = suppress vertical continuation lines */
} ScTreeOpts;

typedef struct ScTreeNode ScTreeNode;
typedef struct ScTree     ScTree;

ScTree     *sc_tree_new     (ScTreeOpts opts);
ScTreeNode *sc_tree_add_str (ScTree *t, ScTreeNode *parent,
                              const char   *str,    ScOptions opts,
                              const char   *prefix, ScOptions prefix_opts);
ScTreeNode *sc_tree_add_text(ScTree *t, ScTreeNode *parent,
                              const ScText *text,
                              const char   *prefix, ScOptions prefix_opts);
void        sc_tree_print   (const ScTree *t);
void        sc_tree_free    (ScTree *t);
