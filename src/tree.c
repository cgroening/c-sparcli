#include "sparcli.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Connector character tables ─────────────────────────────────────────── */

static const struct {
    const char *branch;  /* non-last child:  ├── */
    const char *last;    /* last child:      └── */
    const char *cont;    /* continuation:    │   */
    const char *empty;   /* ended branch:    (spaces) */
} tc[] = {
    [SC_BORDER_NONE]    = { "   ", "   ", "   ", "   " },
    [SC_BORDER_ASCII]   = { "+--", "\\--", "|  ", "   " },
    [SC_BORDER_SINGLE]  = { "├──", "└──", "│  ", "   " },
    [SC_BORDER_DOUBLE]  = { "╠══", "╚══", "║  ", "   " },
    [SC_BORDER_ROUNDED] = { "├──", "└──", "│  ", "   " },
    [SC_BORDER_THICK]   = { "┣━━", "┗━━", "┃  ", "   " },
};

/* ── Internal structures ─────────────────────────────────────────────────── */

struct ScTreeNode {
    int           is_text;
    char         *str;          /* owned copy */
    const ScText *text;         /* not owned */
    ScTextStyle     opts;
    char         *prefix;       /* owned, may be NULL */
    ScTextStyle     prefix_opts;
    ScTreeNode  **children;
    size_t        child_count;
    size_t        child_cap;
};

struct ScTree {
    ScTreeNode  **roots;
    size_t        root_count;
    size_t        root_cap;
    ScTreeOpts    opts;
};

/* ── Node helpers ────────────────────────────────────────────────────────── */

static ScTreeNode *node_alloc(int is_text, const char *str, ScTextStyle opts,
                               const ScText *text,
                               const char *prefix, ScTextStyle prefix_opts) {
    ScTreeNode *n  = calloc(1, sizeof(ScTreeNode));
    n->is_text     = is_text;
    n->str         = str    ? strdup(str)    : NULL;
    n->text        = text;
    n->opts        = opts;
    n->prefix      = prefix ? strdup(prefix) : NULL;
    n->prefix_opts = prefix_opts;
    return n;
}

static void push_child(ScTreeNode *parent, ScTreeNode *child) {
    if (parent->child_count == parent->child_cap) {
        parent->child_cap = parent->child_cap ? parent->child_cap * 2 : 4;
        parent->children  = realloc(parent->children,
                                    parent->child_cap * sizeof(ScTreeNode *));
    }
    parent->children[parent->child_count++] = child;
}

static ScTreeNode *tree_attach(ScTree *t, ScTreeNode *parent, ScTreeNode *n) {
    if (parent) {
        push_child(parent, n);
    } else {
        if (t->root_count == t->root_cap) {
            t->root_cap = t->root_cap ? t->root_cap * 2 : 4;
            t->roots    = realloc(t->roots, t->root_cap * sizeof(ScTreeNode *));
        }
        t->roots[t->root_count++] = n;
    }
    return n;
}

/* ── Public API ──────────────────────────────────────────────────────────── */

ScTree *sc_tree_new(ScTreeOpts opts) {
    ScTree *t = calloc(1, sizeof(ScTree));
    t->opts   = opts;
    return t;
}

ScTreeNode *sc_tree_add_str(ScTree *t, ScTreeNode *parent,
                             const char *str, ScTextStyle opts,
                             const char *prefix, ScTextStyle prefix_opts) {
    return tree_attach(t, parent, node_alloc(0, str, opts, NULL, prefix, prefix_opts));
}

ScTreeNode *sc_tree_add_text(ScTree *t, ScTreeNode *parent,
                              const ScText *text,
                              const char *prefix, ScTextStyle prefix_opts) {
    ScTextStyle none = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
    return tree_attach(t, parent, node_alloc(1, NULL, none, text, prefix, prefix_opts));
}

/* ── Rendering ───────────────────────────────────────────────────────────── */

static void print_conn(const char *s, ScColor color) {
    if (color.index != -2) {
        sc_apply_colors(color, SC_ANSI_COLOR_NONE);
        fputs(s, stdout);
        fputs("\033[0m", stdout);
    } else {
        fputs(s, stdout);
    }
}

/* Builds: existing_prefix + colored_cont_str + indent_spaces */
static char *make_child_prefix(const char *prefix, const char *cont,
                                ScColor color, int indent) {
    size_t plen  = strlen(prefix);
    size_t clen  = strlen(cont);
    /* max ANSI overhead: ~25 chars (RGB fg + reset) */
    size_t alloc = plen + 30 + clen + (size_t)indent + 1;
    char  *buf   = malloc(alloc);
    size_t pos   = 0;

    memcpy(buf + pos, prefix, plen);
    pos += plen;

    if (color.index != -2) {
        int n;
        if (color.index == -1) {
            n = snprintf(buf + pos, 25, "\033[38;2;%d;%d;%dm",
                         color.r, color.g, color.b);
        } else {
            n = snprintf(buf + pos, 10, "\033[3%dm", color.index);
        }
        pos += (size_t)n;
        memcpy(buf + pos, cont, clen); pos += clen;
        memcpy(buf + pos, "\033[0m", 4); pos += 4;
    } else {
        memcpy(buf + pos, cont, clen); pos += clen;
    }

    for (int i = 0; i < indent; i++) { buf[pos++] = ' '; }
    buf[pos] = '\0';
    return buf;
}

static void render_node(const ScTree *t, const ScTreeNode *node,
                        const char *prefix, int depth, int is_last) {
    int style  = (t->opts.style >= SC_BORDER_NONE && t->opts.style <= SC_BORDER_THICK)
                 ? (int)t->opts.style : SC_BORDER_SINGLE;
    int indent = t->opts.indent > 0 ? t->opts.indent : 1;

    /* ── connector ── */
    if (depth > 0) {
        fputs(prefix, stdout);
        print_conn(is_last ? tc[style].last : tc[style].branch,
                   t->opts.connector_color);
        for (int i = 0; i < indent; i++) { fputc(' ', stdout); }
    }

    /* ── prefix icon / marker ── */
    if (node->prefix) {
        sc_print(node->prefix, node->prefix_opts);
    }

    /* ── content ── */
    if (node->is_text) {
        if (node->text) { sc_print_text(node->text); }
    } else {
        if (node->str)  { sc_print(node->str, node->opts); }
    }

    fputc('\n', stdout);

    if (node->child_count == 0) { return; }

    /* ── build child prefix ── */
    char *child_prefix;
    if (depth == 0) {
        child_prefix = strdup("");
    } else {
        int use_cont  = !is_last && !t->opts.no_guide;
        const char *cs = use_cont ? tc[style].cont : tc[style].empty;
        ScColor     cc = use_cont ? t->opts.connector_color : SC_ANSI_COLOR_NONE;
        child_prefix   = make_child_prefix(prefix, cs, cc, indent);
    }

    for (size_t i = 0; i < node->child_count; i++) {
        render_node(t, node->children[i], child_prefix,
                    depth + 1, i == node->child_count - 1);
    }

    free(child_prefix);
}

void sc_tree_print(const ScTree *t) {
    if (!t || !t->root_count) { return; }
    for (size_t i = 0; i < t->root_count; i++) {
        render_node(t, t->roots[i], "", 0, i == t->root_count - 1);
    }
}

/* ── Memory management ───────────────────────────────────────────────────── */

static void node_free(ScTreeNode *n) {
    if (!n) { return; }
    free(n->str);
    free(n->prefix);
    for (size_t i = 0; i < n->child_count; i++) {
        node_free(n->children[i]);
    }
    free(n->children);
    free(n);
}

void sc_tree_free(ScTree *t) {
    if (!t) { return; }
    for (size_t i = 0; i < t->root_count; i++) {
        node_free(t->roots[i]);
    }
    free(t->roots);
    free(t);
}
