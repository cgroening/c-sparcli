#pragma once

#include "sparcli_core.h"
#include "sparcli_text.h"

typedef enum {
    SC_LIST_BULLET,
    SC_LIST_NUMBER,
    SC_LIST_ALPHA_LC,
    SC_LIST_ALPHA_UC,
    SC_LIST_ROMAN_LC,
    SC_LIST_ROMAN_UC,
} ScListMarker;

typedef struct {
    ScListMarker  marker;
    const char   *bullet;
    const char   *marker_prefix;
    const char   *marker_suffix;
    ScTextStyle     marker_opts;
    int           indent;
    int           item_gap;
    int           width;
    ScEdges       margin;        /* top/bottom = blank lines; left/right = outer indent */
} ScListOpts;

typedef struct ScListItem ScListItem;
typedef struct ScList     ScList;

ScList     *sc_list_new     (ScListOpts opts);
ScListItem *sc_list_add_str (ScList *l, const char *str, ScTextStyle opts);
ScListItem *sc_list_add_text(ScList *l, const ScText *t);
ScList     *sc_list_add_sub (ScListItem *parent, ScListOpts opts);
void        sc_list_print   (const ScList *l);
void        sc_list_free    (ScList *l);
