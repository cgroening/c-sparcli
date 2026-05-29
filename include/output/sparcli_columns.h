#pragma once

#include "core/sparcli_core.h"
#include "core/sparcli_rendered.h"
#include "core/sparcli_text.h"
#include "output/sparcli_panel.h"
#include "output/sparcli_table.h"
#include "output/sparcli_tree.h"
#include "output/sparcli_list.h"

SPARCLI_BEGIN_DECLS


/**
 * Per-column options passed alongside a captured widget.
 *
 * Width priority: `fixed_w` (when `> 0`) overrides `min_w`/`max_w`;
 * otherwise the natural content width is clamped to `[min_w, max_w]`
 * when those are set.
 */
typedef struct ScColItem {
    /** Minimum column width in columns; `0` = no minimum. */
    int min_w;

    /** Maximum column width in columns; `0` = no maximum. */
    int max_w;

    /** Fixed column width; `0` = auto. Overrides `min_w`/`max_w` when `> 0`. */
    int fixed_w;

    /** Horizontal placement of the content when the column is wider. */
    ScHAlign halign;

    /** When `true`, `valign` overrides `ScColumnsOpts.valign` for this column. */
    bool valign_set;

    /** Vertical alignment override; effective only when `valign_set` is `true`. */
    ScVAlign valign;

    /**
     * Background color filling padding spaces and empty slots; zero-init =
     * no color. Does not affect the captured widget content itself.
     */
    ScColor bg;

    /** When `true`, extend the captured panel to the full column height. */
    bool stretch;
} ScColItem;

/**
 * Layout options applied to a `ScColumns` container.
 */
typedef struct ScColumnsOpts {
    /**
     * Space between columns; default `3` without a separator, `2` with one
     * (added on both sides of the separator).
     */
    int gap;

    /** Separator style, color and bg; zero-init = no separator. */
    ScBorderStyle sep;

    /** Default vertical alignment per column; overridable per `ScColItem`. */
    ScVAlign valign;

    /** `0` = auto; `>0` = distribute width across flex columns. */
    int total_width;

    /** Outer margin (top/right/bottom/left). */
    ScEdges margin;
} ScColumnsOpts;

/** Opaque columns container; build with `sc_columns_new`. */
typedef struct ScColumns ScColumns;


/**
 * Allocates an empty columns layout.
 *
 * @param opts  Layout options.
 * @return      Heap-allocated layout; free with `sc_columns_free`.
 */
ScColumns *sc_columns_new(ScColumnsOpts opts);

/**
 * Captures a table and appends it as a column.
 *
 * The table is rendered into a temporary buffer **at call time** and the
 * resulting frozen rendering is stored. The caller may freely modify or
 * free `table` afterwards — the columns layout no longer references it.
 *
 * @param columns  Layout the column is appended to.
 * @param table    Source table; rendered at call time, then no longer
 *                 referenced.
 * @param opts     Table rendering options (passed by value).
 * @param item     Per-column options.
 */
SPARCLI_EXPORT void sc_columns_add_table(
    ScColumns *columns, const ScTableData *table,
    ScTableOpts opts, ScColItem item
);

/**
 * Captures a plain-string panel and appends it as a column.
 *
 * Same snapshot semantics as `sc_columns_add_table`: the panel is
 * rendered into a frozen buffer at call time, `content` is no longer
 * referenced after the call.
 */
SPARCLI_EXPORT void sc_columns_add_panel_str(
    ScColumns *columns, const char *content,
    ScPanelOpts opts, ScColItem item
);

/** Same snapshot semantics as `sc_columns_add_panel_str`. */
SPARCLI_EXPORT void sc_columns_add_panel_text(
    ScColumns *columns, const ScText *content,
    ScPanelOpts opts, ScColItem item
);

/** Same snapshot semantics as `sc_columns_add_table`; captures `text`. */
SPARCLI_EXPORT void sc_columns_add_text(
    ScColumns *columns, const ScText *text, ScColItem item
);

/** Same snapshot semantics as `sc_columns_add_table`; captures `str`. */
SPARCLI_EXPORT void sc_columns_add_str(
    ScColumns *columns, const char *str, ScColItem item
);

/**
 * Captures a nested columns layout and appends it as a column.
 *
 * The nested layout is rendered into a frozen buffer at call time; later
 * modifications to `nested` have no effect on the captured snapshot.
 */
SPARCLI_EXPORT void sc_columns_add_columns(
    ScColumns *columns, const ScColumns *nested, ScColItem item
);

/** Same snapshot semantics as `sc_columns_add_table`; captures `tree`. */
SPARCLI_EXPORT void sc_columns_add_tree(
    ScColumns *columns, const ScTree *tree, ScColItem item
);

/** Same snapshot semantics as `sc_columns_add_table`; captures `list`. */
SPARCLI_EXPORT void sc_columns_add_list(
    ScColumns *columns, const ScList *list, ScColItem item
);

/**
 * Appends an already-captured `ScRendered` as a column.
 *
 * The columns layout makes a deep copy, so the caller may free `rendered`
 * immediately after.
 */
SPARCLI_EXPORT void sc_columns_add_rendered(
    ScColumns *columns, const ScRendered *rendered, ScColItem item
);

/**
 * Renders the columns layout to stdout.
 */
SPARCLI_EXPORT void sc_columns_print(const ScColumns *columns);

/**
 * Frees `columns`, all its captured entries and any owned strings.
 *
 * @param columns  Layout to free; safe to pass `NULL`.
 */
SPARCLI_EXPORT void sc_columns_free(ScColumns *columns);

SPARCLI_END_DECLS
