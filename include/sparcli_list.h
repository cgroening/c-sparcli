#pragma once

#include "sparcli_core.h"
#include "sparcli_text.h"


/**
 * Marker style used in front of each list item.
 */
typedef enum {
    SC_LIST_BULLET,   /**< Fixed character (default `•`); see `ScListOpts.bullet` */
    SC_LIST_NUMBER,   /**< `1`, `2`, `3`, … (decimal counter) */
    SC_LIST_ALPHA_LC, /**< `a`, `b`, `c`, … (lowercase Latin letters) */
    SC_LIST_ALPHA_UC, /**< `A`, `B`, `C`, … (uppercase Latin letters) */
    SC_LIST_ROMAN_LC, /**< `i`, `ii`, `iii`, … (lowercase Roman numerals) */
    SC_LIST_ROMAN_UC, /**< `I`, `II`, `III`, … (uppercase Roman numerals) */
} ScListMarker;

/**
 * Rendering options for a list.
 */
typedef struct ScListOpts {
    /** Marker style (bullet, number, letter, Roman). */
    ScListMarker marker;

    /** Bullet character used when `marker == SC_LIST_BULLET`; `NULL` = `•`. */
    const char *bullet;

    /** Text placed before the marker value (e.g. `"("` → `(1`); default `""`. */
    const char *marker_prefix;

    /** Text placed after the marker value (e.g. `"."` → `1.`); default `"."`. */
    const char *marker_suffix;

    /** Style applied to the full marker field; zero-init = no formatting. */
    ScTextStyle marker_style;

    /** Left indent relative to the parent list, in columns. */
    int indent;

    /** Blank lines inserted between items; `0` = none. */
    int item_gap;

    /** Total line width in columns; `0` = terminal width. */
    int width;

    /**
     * Outer margin; top/bottom = blank lines, left/right = outer indent
     * in columns.
     */
    ScEdges margin;
} ScListOpts;

/** Opaque list container; build with `sc_list_new`. */
typedef struct ScList ScList;

/** Opaque list item returned by the `sc_list_add_*` functions. */
typedef struct ScListItem ScListItem;


/**
 * Allocates an empty list.
 *
 * @param opts  Rendering options (marker, indent, width, …).
 * @return      Heap-allocated list; free with `sc_list_free`.
 */
ScList *sc_list_new(ScListOpts opts);

/**
 * Appends a plain-string item to `list`.
 *
 * @param list   List the item is appended to.
 * @param str    Item text; copied internally.
 * @param style  Style applied to `str`.
 * @return       Pointer to the new item; owned by the list.
 */
ScListItem *sc_list_add_str(ScList *list, const char *str, ScTextStyle style);

/**
 * Appends a rich-text item to `list`.
 *
 * @param list  List the item is appended to.
 * @param text  Rich-text content; not owned by the list.
 * @return      Pointer to the new item; owned by the list.
 */
ScListItem *sc_list_add_text(ScList *list, const ScText *text);

/**
 * Attaches a sub-list to `parent`.
 *
 * Replaces any previously attached sub-list. The returned list is owned by
 * `parent` and is freed together with the outer list.
 *
 * @param parent  Item the sub-list is attached to.
 * @param opts    Rendering options for the sub-list.
 * @return        Newly allocated sub-list; owned by `parent`.
 */
ScList *sc_list_add_sub(ScListItem *parent, ScListOpts opts);

/**
 * Renders `list` to stdout.
 *
 * @param list  List to render; no output when `list` is `NULL`.
 */
void sc_list_print(const ScList *list);

/**
 * Frees `list`, all its items and any attached sub-lists.
 *
 * @param list  List to free; safe to pass `NULL`.
 */
void sc_list_free(ScList *list);
