#include "sparcli.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** Box-drawing characters for the four tree connector positions. */
typedef struct ConnectorSet {
    /** Branch leading to a non-last sibling (e.g. `├──`). */
    const char *branch;

    /** Branch leading to the last sibling at the current depth (e.g. `└──`). */
    const char *last_child;

    /** Vertical guide drawn under an unfinished branch (e.g. `│  `). */
    const char *guide;

    /** Blank spacer drawn under a finished branch (3 spaces). */
    const char *gap;
} ConnectorSet;

/** One node in the tree. */
struct ScTreeNode {
    /** `true` = `text` is used as content; `false` = `str` is used. */
    bool is_text;

    /** Plain-string content; owned copy when set; `NULL` for text nodes. */
    char *str;

    /** Rich-text content; not owned; `NULL` for string nodes. */
    const ScText *text;

    /** Style applied to `str`; ignored for text nodes. */
    ScTextStyle style;

    /** Optional marker rendered before the node text; owned; may be `NULL`. */
    char *prefix;

    /** Style applied to `prefix`. */
    ScTextStyle prefix_style;

    /** Heap-allocated array of child node pointers; owned. */
    struct ScTreeNode **children;

    /** Number of valid entries in `children`. */
    size_t child_count;

    /** Allocated capacity of `children`. */
    size_t child_capacity;
};

/** Tree container. */
struct ScTree {
    /** Heap-allocated array of root node pointers; owned. */
    struct ScTreeNode **roots;

    /** Number of valid entries in `roots`. */
    size_t root_count;

    /** Allocated capacity of `roots`. */
    size_t root_capacity;

    /** Rendering options provided at construction. */
    ScTreeOpts opts;
};

/** Render-time context for a single tree print call. */
typedef struct Tree {
    /** Tree being rendered; not owned. */
    const ScTree *tree;

    /** Box-drawing connectors selected from `connector_table`. */
    ConnectorSet connectors;

    /** Spaces inserted between the connector and the node content. */
    int connector_indent;
} Tree;

/** Per-call inputs for a single recursive `render_node` invocation. */
typedef struct NodeFrame {
    /** Node to render. */
    const ScTreeNode *node;

    /** Prefix string accumulated from ancestor connectors. */
    const char *prefix;

    /** Depth in the tree; root nodes have depth `0`. */
    int depth;

    /** `true` when `node` is the last child at its depth. */
    bool is_last_sibling;
} NodeFrame;


static const ConnectorSet connector_table[] = {
    [SC_BORDER_NONE]    = { "   ", "   ",   "   ", "   " },
    [SC_BORDER_ASCII]   = { "+--", "\\--",  "|  ", "   " },
    [SC_BORDER_SINGLE]  = { "├──", "└──",   "│  ", "   " },
    [SC_BORDER_DOUBLE]  = { "╠══", "╚══",   "║  ", "   " },
    [SC_BORDER_ROUNDED] = { "├──", "└──",   "│  ", "   " },
    [SC_BORDER_THICK]   = { "┣━━", "┗━━",   "┃  ", "   " },
};


// Forward declarations indented to reflect call hierarchy
static ScTreeNode *new_node(
    bool is_text, const char *str, ScTextStyle style,
    const ScText *text, const char *prefix, ScTextStyle prefix_style
);
static ScTreeNode *attach_node(
    ScTree *tree, ScTreeNode *parent, ScTreeNode *node
);
    static void append_child(ScTreeNode *parent, ScTreeNode *child);
    static void append_root(ScTree *tree, ScTreeNode *node);

static void init_tree(Tree *self, const ScTree *tree);
    static int get_border_type(const ScTreeOpts *opts);
    static int get_indent(const ScTreeOpts *opts);

static void render_node(Tree *self, NodeFrame frame);
    static void print_connector(Tree *self, NodeFrame frame);
        static void print_colored(const char *str, ScColor color);
        static void print_spaces(int count);
    static void print_node_content(const ScTreeNode *node);
    static char *build_child_prefix(Tree *self, NodeFrame frame);

static void free_node(ScTreeNode *node);


ScTree *sc_tree_new(ScTreeOpts opts) {
    ScTree *tree = calloc(1, sizeof(ScTree));
    tree->opts = opts;
    return tree;
}

ScTreeNode *sc_tree_add_str(
    ScTree *tree, ScTreeNode *parent,
    const char *str, ScTextStyle style,
    const char *prefix, ScTextStyle prefix_style
) {
    if (!tree) { return NULL; }
    ScTreeNode *node = new_node(
        false, str, style, NULL, prefix, prefix_style
    );
    return attach_node(tree, parent, node);
}

ScTreeNode *sc_tree_add_text(
    ScTree *tree, ScTreeNode *parent,
    const ScText *text,
    const char *prefix, ScTextStyle prefix_style
) {
    if (!tree) { return NULL; }
    ScTextStyle no_style = {
        SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
    };
    ScTreeNode *node = new_node(
        true, NULL, no_style, text, prefix, prefix_style
    );
    return attach_node(tree, parent, node);
}

void sc_tree_print(const ScTree *tree) {
    if (!tree || tree->root_count == 0) { return; }

    Tree self;
    init_tree(&self, tree);

    for (size_t i = 0; i < tree->root_count; i++) {
        NodeFrame frame = {
            .node = tree->roots[i],
            .prefix = "",
            .depth = 0,
            .is_last_sibling = (i == tree->root_count - 1),
        };
        render_node(&self, frame);
    }
}

void sc_tree_free(ScTree *tree) {
    if (!tree) { return; }
    for (size_t i = 0; i < tree->root_count; i++) {
        free_node(tree->roots[i]);
    }
    free(tree->roots);
    free(tree);
}


/** Allocates a new node and copies the string fields it owns. */
static ScTreeNode *new_node(
    bool is_text, const char *str, ScTextStyle style,
    const ScText *text, const char *prefix, ScTextStyle prefix_style
) {
    ScTreeNode *node = calloc(1, sizeof(ScTreeNode));
    node->is_text = is_text;
    node->str = str ? strdup(str) : NULL;
    node->text = text;
    node->style = style;
    node->prefix = prefix ? strdup(prefix) : NULL;
    node->prefix_style = prefix_style;
    return node;
}

/**
 * Attaches `node` as a child of `parent`, or as a new root when
 * `parent` is `NULL`.
 */
static ScTreeNode *attach_node(
    ScTree *tree, ScTreeNode *parent, ScTreeNode *node
) {
    if (parent) {
        append_child(parent, node);
    } else {
        append_root(tree, node);
    }
    return node;
}

/** Appends `child` to `parent`, growing the child array as needed. */
static void append_child(ScTreeNode *parent, ScTreeNode *child) {
    if (parent->child_count == parent->child_capacity) {
        parent->child_capacity = parent->child_capacity
            ? parent->child_capacity * 2 : 4;
        parent->children = realloc(
            parent->children,
            parent->child_capacity * sizeof(ScTreeNode *)
        );
    }
    parent->children[parent->child_count++] = child;
}

/** Appends `node` to `tree->roots`, growing the array as needed. */
static void append_root(ScTree *tree, ScTreeNode *node) {
    if (tree->root_count == tree->root_capacity) {
        tree->root_capacity = tree->root_capacity
            ? tree->root_capacity * 2 : 4;
        tree->roots = realloc(
            tree->roots, tree->root_capacity * sizeof(ScTreeNode *)
        );
    }
    tree->roots[tree->root_count++] = node;
}


/** Populates `self` from `tree` for the duration of one render pass. */
static void init_tree(Tree *self, const ScTree *tree) {
    self->tree = tree;
    self->connectors = connector_table[get_border_type(&tree->opts)];
    self->connector_indent = get_indent(&tree->opts);
}

/**
 * Returns the validated border type for connector lookup; falls back to
 * `SC_BORDER_SINGLE` when `opts->border_type` is out of range.
 */
static int get_border_type(const ScTreeOpts *opts) {
    if (opts->border_type >= SC_BORDER_NONE
        && opts->border_type <= SC_BORDER_THICK) {
        return opts->border_type;
    }
    return SC_BORDER_SINGLE;
}

/** Returns the spaces inserted after the connector; default `1`. */
static int get_indent(const ScTreeOpts *opts) {
    return opts->indent > 0 ? opts->indent : 1;
}


/**
 * Renders `frame.node` to stdout, then recurses into its children.
 *
 * @param frame  Per-call inputs: target node, accumulated prefix, depth
 *               and last-sibling flag.
 */
static void render_node(Tree *self, NodeFrame frame) {
    if (frame.depth > 0) {
        print_connector(self, frame);
    }
    print_node_content(frame.node);
    fputc('\n', sc_output_stream());

    if (frame.node->child_count == 0) { return; }

    char *child_prefix = build_child_prefix(self, frame);
    for (size_t i = 0; i < frame.node->child_count; i++) {
        NodeFrame child_frame = {
            .node = frame.node->children[i],
            .prefix = child_prefix,
            .depth = frame.depth + 1,
            .is_last_sibling = (i == frame.node->child_count - 1),
        };
        render_node(self, child_frame);
    }
    free(child_prefix);
}

/**
 * Prints the ancestor prefix followed by the connector for the current
 * node and the configured spaces after it.
 */
static void print_connector(Tree *self, NodeFrame frame) {
    fputs(frame.prefix, sc_output_stream());
    const char *connector = frame.is_last_sibling
        ? self->connectors.last_child : self->connectors.branch;
    print_colored(connector, self->tree->opts.connector_color);
    print_spaces(self->connector_indent);
}

/**
 * Prints `str` wrapped in ANSI color escapes when `color` is set; prints
 * it as-is when `color` is `SC_ANSI_COLOR_NONE`.
 */
static void print_colored(const char *str, ScColor color) {
    if (color.index == -2) {
        fputs(str, sc_output_stream());
        return;
    }
    sc_apply_colors(color, SC_ANSI_COLOR_NONE);
    fputs(str, sc_output_stream());
    fputs(SC_ANSI_ESCAPE_CODE_RESET, sc_output_stream());
}

/** Prints `count` space characters to stdout. */
static void print_spaces(int count) {
    for (int i = 0; i < count; i++) { fputc(' ', sc_output_stream()); }
}

/**
 * Prints the optional prefix marker followed by the node's content
 * (`str` or `text` depending on `node->is_text`).
 */
static void print_node_content(const ScTreeNode *node) {
    if (node->prefix) {
        sc_print(node->prefix, node->prefix_style);
    }
    if (node->is_text) {
        if (node->text) { sc_print_text(node->text); }
    } else if (node->str) {
        sc_print(node->str, node->style);
    }
}

/**
 * Builds the prefix string used by children of `frame.node`: the parent
 * prefix plus either a colored vertical guide (when more siblings follow
 * and guides are enabled) or a blank gap. Caller owns the returned buffer.
 */
static char *build_child_prefix(Tree *self, NodeFrame frame) {
    if (frame.depth == 0) { return strdup(""); }

    bool show_guide =
        !frame.is_last_sibling && !self->tree->opts.no_guide;
    const char *segment = show_guide
        ? self->connectors.guide : self->connectors.gap;
    ScColor segment_color = show_guide
        ? self->tree->opts.connector_color : SC_ANSI_COLOR_NONE;

    // Reserve space: parent prefix + ANSI fg escape (~20) + segment
    // + reset (4) + trailing indent spaces + terminator.
    size_t parent_length = strlen(frame.prefix);
    size_t segment_length = strlen(segment);
    int indent = self->connector_indent;
    size_t buffer_size = parent_length + 30
                        + segment_length + (size_t)indent + 1;
    char *buffer = malloc(buffer_size);
    size_t position = 0;

    memcpy(buffer + position, frame.prefix, parent_length);
    position += parent_length;

    if (segment_color.index != -2) {
        int written;
        if (segment_color.index == -1) {
            written = snprintf(
                buffer + position, 25, "\033[38;2;%d;%d;%dm",
                segment_color.r, segment_color.g, segment_color.b
            );
        } else {
            written = snprintf(
                buffer + position, 10, "\033[3%dm", segment_color.index
            );
        }
        position += (size_t)written;
        memcpy(buffer + position, segment, segment_length);
        position += segment_length;
        memcpy(buffer + position, "\033[0m", 4);
        position += 4;
    } else {
        memcpy(buffer + position, segment, segment_length);
        position += segment_length;
    }

    for (int i = 0; i < indent; i++) { buffer[position++] = ' '; }
    buffer[position] = '\0';
    return buffer;
}


/** Recursively frees `node`, its children and its owned strings. */
static void free_node(ScTreeNode *node) {
    if (!node) { return; }
    free(node->str);
    free(node->prefix);
    for (size_t i = 0; i < node->child_count; i++) {
        free_node(node->children[i]);
    }
    free(node->children);
    free(node);
}
