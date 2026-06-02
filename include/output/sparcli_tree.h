#pragma once

#include "core/sparcli_core.h"
#include "core/sparcli_text.h"

SPARCLI_BEGIN_DECLS


/**
 * Rendering options for a tree layout.
 */
typedef struct ScTreeOpts {
    /** Connector style; selects the box-drawing set for branches and guides. */
    ScBorderType type;

    /** Connector color; `SC_ANSI_COLOR_NONE` = no escape codes. */
    ScColor connector_color;

    /** Spaces between the connector and the node text; default `1`. */
    int indent;

    /** When `true`, suppress vertical continuation guides under finished
        branches. */
    bool no_guide;

    /**
     * ANSI passthrough for node strings; zero-init inherits the
     * `sc_set_allow_ansi` global.
     */
    ScAnsiMode ansi;
} ScTreeOpts;

/** Opaque tree container; build with `sc_tree_new`. */
typedef struct ScTree ScTree;

/** Opaque tree node returned by the `sc_tree_add_*` functions. */
typedef struct ScTreeNode ScTreeNode;


/**
 * Allocates an empty tree.
 *
 * @param opts  Rendering options (style, color, indent, guide).
 * @return      Heap-allocated tree; free with `sc_tree_free`.
 */
ScTree *sc_tree_new(ScTreeOpts opts);

/**
 * Attaches a plain-string node to `tree`.
 *
 * @param tree         Tree the node is added to.
 * @param parent       Parent node; `NULL` adds a new root.
 * @param str          Node text; copied internally.
 * @param style        Style applied to `str`.
 * @param prefix       Optional marker printed before `str`; may be `NULL`.
 * @param prefix_style Style applied to `prefix`.
 * @return             Pointer to the new node; owned by the tree.
 */
ScTreeNode *sc_tree_add_str(
    ScTree *tree, ScTreeNode *parent,
    const char *str, ScTextStyle style,
    const char *prefix, ScTextStyle prefix_style
);

/**
 * Attaches a rich-text node to `tree`.
 *
 * @param tree         Tree the node is added to.
 * @param parent       Parent node; `NULL` adds a new root.
 * @param text         Rich-text content; not owned by the tree.
 * @param prefix       Optional marker printed before `text`; may be `NULL`.
 * @param prefix_style Style applied to `prefix`.
 * @return             Pointer to the new node; owned by the tree.
 */
ScTreeNode *sc_tree_add_text(
    ScTree *tree, ScTreeNode *parent,
    const ScText *text,
    const char *prefix, ScTextStyle prefix_style
);

/**
 * Renders `tree` to stdout.
 *
 * @param tree  Tree to render; no output when `tree` is `NULL` or empty.
 */
SPARCLI_EXPORT void sc_tree_print(const ScTree *tree);

/**
 * Frees `tree`, all its nodes, and any owned strings.
 *
 * @param tree  Tree to free; safe to pass `NULL`.
 */
SPARCLI_EXPORT void sc_tree_free(ScTree *tree);

SPARCLI_END_DECLS
