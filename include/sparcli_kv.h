#pragma once

#include "sparcli_core.h"


/**
 * Rendering options for a key-value list.
 */
typedef struct ScKVOpts {
    /**
     * Separator printed between the padded key and the value; `NULL` = `"  "`.
     */
    const char *sep;

    /** Fixed key column width in columns; `0` = auto-fit to the widest key. */
    int key_width;

    /** Total line width in columns; `0` = terminal width. */
    int width;

    /**
     * Outer margin; top/bottom = blank lines, left/right = outer indent
     * in columns.
     */
    ScEdges margin;

    /** Blank lines inserted between items; `0` = none. */
    int item_gap;

    /** When `true`, word-wrap long values; when `false`, truncate. */
    bool wrap_val;

    /** Style applied to keys; zero-init = no formatting. */
    ScTextStyle key_style;

    /** Style applied to values; zero-init = no formatting. */
    ScTextStyle val_style;
} ScKVOpts;

/** Opaque key-value list; build with `sc_kv_new`. */
typedef struct ScKV ScKV;


/**
 * Allocates an empty key-value list.
 *
 * @param opts  Rendering options.
 * @return      Heap-allocated list; free with `sc_kv_free`.
 */
ScKV *sc_kv_new(ScKVOpts opts);

/**
 * Appends one key-value pair to `kv`.
 *
 * @param kv     List the pair is appended to.
 * @param key    Key string; copied internally; `NULL` is treated as `""`.
 * @param value  Value string; copied internally; `NULL` is treated as `""`.
 */
void sc_kv_add(ScKV *kv, const char *key, const char *value);

/**
 * Renders `kv` to stdout.
 *
 * Layout per line: `margin + key (padded to key_w) + sep + value + margin`.
 * Continuation lines of a wrapped value are indented to align under the
 * value column.
 *
 * @param kv  List to render; no output when `kv` is `NULL` or empty.
 */
void sc_kv_print(const ScKV *kv);

/**
 * Frees `kv`, all its entries and their owned strings.
 *
 * @param kv  List to free; safe to pass `NULL`.
 */
void sc_kv_free(ScKV *kv);
