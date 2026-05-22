#pragma once

#include "sparcli_core.h"
#include "sparcli_rendered.h"
#include "sparcli_text.h"
#include "sparcli_panel.h"
#include "sparcli_table.h"
#include "sparcli_tree.h"
#include "sparcli_list.h"

typedef struct {
    int      min_w;
    int      max_w;
    int      fixed_w;
    ScHAlign  align;      /* content placement when col_w > content_w */
    int      valign_set; /* 1 = override ScColumnsOpts.valign for this column */
    ScVAlign valign;
} ScColItem;

typedef struct {
    int           gap;         /* spaces between columns, default 3 */
    ScBorderType sep_style;
    ScColor       sep_color;
    ScVAlign      valign;
    int           total_width; /* 0 = auto; >0 = scale flex cols to fit */
} ScColumnsOpts;

typedef struct ScColumns ScColumns;

ScColumns *sc_columns_new         (ScColumnsOpts opts);
void sc_columns_add_table         (ScColumns *cl, const ScTable   *t,       ScColItem item);
void sc_columns_add_panel_str     (ScColumns *cl, const char      *content, ScPanelOpts opts, ScColItem item);
void sc_columns_add_panel_text    (ScColumns *cl, const ScText    *content, ScPanelOpts opts, ScColItem item);
void sc_columns_add_text          (ScColumns *cl, const ScText    *t,       ScColItem item);
void sc_columns_add_str           (ScColumns *cl, const char      *s,       ScColItem item);
void sc_columns_add_columns       (ScColumns *cl, const ScColumns *nested,  ScColItem item);
void sc_columns_add_tree          (ScColumns *cl, const ScTree    *tree,    ScColItem item);
void sc_columns_add_list          (ScColumns *cl, const ScList    *list,    ScColItem item);
void sc_columns_add_rendered      (ScColumns *cl, const ScRendered *r,      ScColItem item);
void sc_columns_print             (const ScColumns *cl);
void sc_columns_free              (ScColumns *cl);
