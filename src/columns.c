#include "sparcli.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>


/** Initial capacity used when growing the line buffer in a captured render. */
#define INITIAL_LINE_CAPACITY 16

/** Initial capacity for the entries array of an `ScColumns`. */
#define INITIAL_ENTRY_CAPACITY 4

/** Default gap when no separator is configured. */
#define DEFAULT_GAP_NO_SEPARATOR 3

/** Default gap when a separator is configured (spaces on each side). */
#define DEFAULT_GAP_WITH_SEPARATOR 2


static const char *separator_chars[] = {
    [SC_BORDER_NONE]    = NULL,
    [SC_BORDER_ASCII]   = "|",
    [SC_BORDER_SINGLE]  = "│",
    [SC_BORDER_DOUBLE]  = "║",
    [SC_BORDER_ROUNDED] = "│",
    [SC_BORDER_THICK]   = "┃",
};


/** One captured column entry. */
typedef struct ScColEntry {
    /** Captured widget rendering; owned by the entry. */
    ScRendered *rendered;

    /** Per-column layout options. */
    ScColItem item;

    /** When `true`, the entry is extended to the layout's max height. */
    bool stretch;

    /**
     * Panel options remembered when the entry was added; used to generate
     * matching filler lines when `stretch` is on.
     */
    ScPanelOpts panel_opts;
} ScColEntry;

struct ScColumns {
    /** Heap-allocated array of column entries. */
    ScColEntry *entries;

    /** Number of valid entries. */
    size_t count;

    /** Allocated capacity of `entries`. */
    size_t capacity;

    /** Layout options. */
    ScColumnsOpts opts;
};

/** Per-print render context built from an `ScColumns`. */
typedef struct ColumnsRender {
    /** Layout being rendered; not owned. */
    const ScColumns *columns;

    /** Resolved separator string; `NULL` when no separator is configured. */
    const char *separator;

    /** Resolved gap (spaces between columns). */
    int gap;

    /** Resolved left outer margin. */
    int left_margin;

    /** Working list of rendered widgets (possibly stretched copies). */
    ScRendered **working;

    /** Per-column rendered widths. */
    int *column_widths;

    /** Per-column top offsets for vertical alignment. */
    int *top_offsets;

    /** Maximum line count across all working entries. */
    size_t total_height;
} ColumnsRender;


// Forward declarations indented to reflect call hierarchy
static int ansi_visible_width(const char *str);
static bool color_is_active(ScColor color);
static void print_spaces(int count);
static void print_newlines(int count);

static ScRendered *capture_render(void (*render_fn)(void *), void *ctx);
    static ScRendered *buffer_to_rendered(const char *buffer, size_t size);

static void push_entry(ScColumns *columns, ScRendered *rendered, ScColItem item);
static void push_panel_entry(
    ScColumns *columns, ScRendered *rendered,
    ScColItem item, ScPanelOpts panel_opts
);

static void init_render(ColumnsRender *self, const ScColumns *columns);
    static size_t compute_max_height(const ScColumns *columns);
    static void apply_stretch(ColumnsRender *self, size_t max_height);
        static ScRendered *stretch_rendered(
            const ScRendered *source, int extra_lines,
            const ScPanelOpts *opts
        );
            static char *make_empty_panel_line(
                const ScPanelOpts *opts, int inner_width
            );
    static void compute_column_widths(ColumnsRender *self);
    static void distribute_total_width(ColumnsRender *self);
    static void compute_vertical_offsets(ColumnsRender *self);
        static int get_vertical_offset(
            ScVAlign valign, int column_height, int total_height
        );

static void render_all_lines(const ColumnsRender *self);
    static void render_line(const ColumnsRender *self, size_t line_index);
        static void render_cell(
            const ColumnsRender *self, size_t column_index, size_t line_index
        );
            static int get_relative_line_index(
                const ColumnsRender *self,
                size_t column_index, size_t line_index
            );
            static void render_content_cell(
                const char *line, int content_width, int column_width,
                ScColItem item
            );
                static void print_line_with_bg(const char *line, ScColor bg);
            static void render_empty_cell(int column_width, ScColor bg);
        static void render_separator_or_gap(const ColumnsRender *self);
            static void render_separator(
                const ColumnsRender *self, const char *separator
            );
            static void render_gap_spaces(const ColumnsRender *self);

static void cleanup_render(ColumnsRender *self);


/* ── ANSI / color helpers ────────────────────────────────────────────────── */

/**
 * Returns the visible column width of `str`, skipping CSI escape sequences
 * and counting one column per UTF-8 codepoint.
 */
static int ansi_visible_width(const char *str) {
    int width = 0;
    while (*str) {
        if ((unsigned char)*str == 0x1B && str[1] == '[') {
            str += 2;
            while (*str && *str != 'm') { str++; }
            if (*str) { str++; }
            continue;
        }
        if (((unsigned char)*str & 0xC0) != 0x80) { width++; }
        str++;
    }
    return width;
}

/**
 * Returns `true` when `color` should emit ANSI escapes. Both
 * `SC_ANSI_COLOR_NONE` and a zero-initialized `ScColor` return `false`.
 */
static bool color_is_active(ScColor color) {
    if (color.index == -2) { return false; }
    bool is_zero_init = color.index == 0
        && color.r == 0 && color.g == 0 && color.b == 0;
    return !is_zero_init;
}

/** Prints `count` space characters to stdout. */
static void print_spaces(int count) {
    for (int i = 0; i < count; i++) { fputc(' ', stdout); }
}

/** Prints `count` newline characters to stdout. */
static void print_newlines(int count) {
    for (int i = 0; i < count; i++) { fputc('\n', stdout); }
}


/* ── ScRendered ──────────────────────────────────────────────────────────── */

void sc_rendered_free(ScRendered *rendered) {
    if (!rendered) { return; }
    for (size_t i = 0; i < rendered->line_count; i++) {
        free(rendered->lines[i]);
    }
    free(rendered->lines);
    free(rendered->column_widths);
    free(rendered);
}

/**
 * Splits `buffer` into lines and packs them into a new `ScRendered`,
 * computing each line's visible column count.
 */
static ScRendered *buffer_to_rendered(const char *buffer, size_t size) {
    ScRendered *rendered = malloc(sizeof(ScRendered));
    rendered->line_count = 0;
    rendered->max_column_width = 0;

    size_t capacity = INITIAL_LINE_CAPACITY;
    rendered->lines = malloc(capacity * sizeof(char *));
    rendered->column_widths = malloc(capacity * sizeof(int));

    const char *cursor = buffer;
    const char *end = buffer + size;
    while (cursor <= end) {
        const char *newline = memchr(cursor, '\n', (size_t)(end - cursor));
        size_t line_length = newline
            ? (size_t)(newline - cursor) : (size_t)(end - cursor);

        if (!newline && line_length == 0) { break; }

        if (rendered->line_count == capacity) {
            capacity *= 2;
            rendered->lines = realloc(
                rendered->lines, capacity * sizeof(char *)
            );
            rendered->column_widths = realloc(
                rendered->column_widths, capacity * sizeof(int)
            );
        }
        char *line = strndup(cursor, line_length);
        int visible_width = ansi_visible_width(line);

        rendered->lines[rendered->line_count] = line;
        rendered->column_widths[rendered->line_count] = visible_width;
        if (visible_width > rendered->max_column_width) {
            rendered->max_column_width = visible_width;
        }
        rendered->line_count++;
        cursor = newline ? newline + 1 : end + 1;
    }
    return rendered;
}


/* ── Capture-and-replay ──────────────────────────────────────────────────── */

/**
 * Redirects stdout to a `tmpfile()`, invokes `render_fn(ctx)`, restores
 * stdout, and returns the captured output as an `ScRendered`.
 */
static ScRendered *capture_render(void (*render_fn)(void *), void *ctx) {
    fflush(stdout);

    FILE *tmp = tmpfile();
    if (!tmp) { return NULL; }

    int saved_stdout = dup(STDOUT_FILENO);
    dup2(fileno(tmp), STDOUT_FILENO);

    render_fn(ctx);

    fflush(stdout);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);

    fseek(tmp, 0, SEEK_END);
    long size = ftell(tmp);
    rewind(tmp);
    char *buffer = malloc((size_t)size + 1);
    fread(buffer, 1, (size_t)size, tmp);
    buffer[size] = '\0';
    fclose(tmp);

    ScRendered *rendered = buffer_to_rendered(buffer, (size_t)size);
    free(buffer);
    return rendered;
}


/* ── Render thunks for each widget type ──────────────────────────────────── */

typedef struct { const ScTableData *table; ScTableOpts opts; } CtxTable;
static void render_table_ctx(void *p) {
    CtxTable *ctx = p;
    sc_table_print(ctx->table, ctx->opts);
}

typedef struct { const char *content; ScPanelOpts opts; } CtxPanelStr;
static void render_panel_str_ctx(void *p) {
    CtxPanelStr *ctx = p;
    sc_panel_str(ctx->content, ctx->opts);
}

typedef struct { const ScText *content; ScPanelOpts opts; } CtxPanelText;
static void render_panel_text_ctx(void *p) {
    CtxPanelText *ctx = p;
    sc_panel_text(ctx->content, ctx->opts);
}

typedef struct { const ScText *text; } CtxText;
static void render_text_ctx(void *p) {
    sc_print_text(((CtxText *)p)->text);
}

typedef struct { const char *str; } CtxStr;
static void render_str_ctx(void *p) {
    const char *str = ((CtxStr *)p)->str;
    fputs(str, stdout);
    if (str[0] && str[strlen(str) - 1] != '\n') { fputc('\n', stdout); }
}

typedef struct { const ScColumns *columns; } CtxColumns;
static void render_columns_ctx(void *p) {
    sc_columns_print(((CtxColumns *)p)->columns);
}

typedef struct { const ScTree *tree; } CtxTree;
static void render_tree_ctx(void *p) {
    sc_tree_print(((CtxTree *)p)->tree);
}

typedef struct { const ScList *list; } CtxList;
static void render_list_ctx(void *p) {
    sc_list_print(((CtxList *)p)->list);
}

typedef struct { const ScKV *kv; } CtxKV;
static void render_kv_ctx(void *p) {
    sc_kv_print(((CtxKV *)p)->kv);
}

typedef struct { const char *title; ScRuleOpts opts; } CtxRuleStr;
static void render_rule_str_ctx(void *p) {
    CtxRuleStr *ctx = p;
    sc_rule_str(ctx->title, ctx->opts);
}

typedef struct { const ScText *title; ScRuleOpts opts; } CtxRuleText;
static void render_rule_text_ctx(void *p) {
    CtxRuleText *ctx = p;
    sc_rule_text(ctx->title, ctx->opts);
}


/* ── ScColumns construction ──────────────────────────────────────────────── */

ScColumns *sc_columns_new(ScColumnsOpts opts) {
    ScColumns *columns = malloc(sizeof(ScColumns));
    columns->entries = NULL;
    columns->count = 0;
    columns->capacity = 0;
    columns->opts = opts;
    return columns;
}

/** Appends a non-panel rendered entry to `columns`. */
static void push_entry(
    ScColumns *columns, ScRendered *rendered, ScColItem item
) {
    if (columns->count == columns->capacity) {
        columns->capacity = columns->capacity
            ? columns->capacity * 2 : INITIAL_ENTRY_CAPACITY;
        columns->entries = realloc(
            columns->entries, columns->capacity * sizeof(ScColEntry)
        );
    }
    columns->entries[columns->count++] = (ScColEntry){
        rendered, item, false, (ScPanelOpts){ 0 }
    };
}

/** Appends a panel entry with its panel options retained (for stretch). */
static void push_panel_entry(
    ScColumns *columns, ScRendered *rendered,
    ScColItem item, ScPanelOpts panel_opts
) {
    if (columns->count == columns->capacity) {
        columns->capacity = columns->capacity
            ? columns->capacity * 2 : INITIAL_ENTRY_CAPACITY;
        columns->entries = realloc(
            columns->entries, columns->capacity * sizeof(ScColEntry)
        );
    }
    columns->entries[columns->count++] = (ScColEntry){
        rendered, item, item.stretch, panel_opts
    };
}

void sc_columns_add_table(
    ScColumns *columns, const ScTableData *table,
    ScTableOpts opts, ScColItem item
) {
    CtxTable ctx = { table, opts };
    push_entry(columns, capture_render(render_table_ctx, &ctx), item);
}

void sc_columns_add_panel_str(
    ScColumns *columns, const char *content,
    ScPanelOpts opts, ScColItem item
) {
    CtxPanelStr ctx = { content, opts };
    push_panel_entry(
        columns, capture_render(render_panel_str_ctx, &ctx), item, opts
    );
}

void sc_columns_add_panel_text(
    ScColumns *columns, const ScText *content,
    ScPanelOpts opts, ScColItem item
) {
    CtxPanelText ctx = { content, opts };
    push_panel_entry(
        columns, capture_render(render_panel_text_ctx, &ctx), item, opts
    );
}

void sc_columns_add_text(
    ScColumns *columns, const ScText *text, ScColItem item
) {
    CtxText ctx = { text };
    push_entry(columns, capture_render(render_text_ctx, &ctx), item);
}

void sc_columns_add_str(
    ScColumns *columns, const char *str, ScColItem item
) {
    CtxStr ctx = { str };
    push_entry(columns, capture_render(render_str_ctx, &ctx), item);
}

void sc_columns_add_columns(
    ScColumns *columns, const ScColumns *nested, ScColItem item
) {
    CtxColumns ctx = { nested };
    push_entry(columns, capture_render(render_columns_ctx, &ctx), item);
}

void sc_columns_add_tree(
    ScColumns *columns, const ScTree *tree, ScColItem item
) {
    CtxTree ctx = { tree };
    push_entry(columns, capture_render(render_tree_ctx, &ctx), item);
}

void sc_columns_add_list(
    ScColumns *columns, const ScList *list, ScColItem item
) {
    CtxList ctx = { list };
    push_entry(columns, capture_render(render_list_ctx, &ctx), item);
}

void sc_columns_add_rendered(
    ScColumns *columns, const ScRendered *rendered, ScColItem item
) {
    if (!rendered) { return; }

    ScRendered *copy = malloc(sizeof(ScRendered));
    copy->line_count = rendered->line_count;
    copy->max_column_width = rendered->max_column_width;
    copy->lines = malloc(rendered->line_count * sizeof(char *));
    copy->column_widths = malloc(rendered->line_count * sizeof(int));
    for (size_t i = 0; i < rendered->line_count; i++) {
        copy->lines[i] = strdup(rendered->lines[i]);
        copy->column_widths[i] = rendered->column_widths[i];
    }
    push_entry(columns, copy, item);
}

void sc_columns_free(ScColumns *columns) {
    if (!columns) { return; }
    for (size_t i = 0; i < columns->count; i++) {
        sc_rendered_free(columns->entries[i].rendered);
    }
    free(columns->entries);
    free(columns);
}


/* ── Capture API (public) ────────────────────────────────────────────────── */

ScRendered *sc_capture_str(const char *str) {
    CtxStr ctx = { str };
    return capture_render(render_str_ctx, &ctx);
}

ScRendered *sc_capture_text(const ScText *text) {
    CtxText ctx = { text };
    return capture_render(render_text_ctx, &ctx);
}

ScRendered *sc_capture_table(const ScTableData *table, ScTableOpts opts) {
    CtxTable ctx = { table, opts };
    return capture_render(render_table_ctx, &ctx);
}

ScRendered *sc_capture_list(const ScList *list) {
    CtxList ctx = { list };
    return capture_render(render_list_ctx, &ctx);
}

ScRendered *sc_capture_tree(const ScTree *tree) {
    CtxTree ctx = { tree };
    return capture_render(render_tree_ctx, &ctx);
}

ScRendered *sc_capture_kv(const ScKV *kv) {
    CtxKV ctx = { kv };
    return capture_render(render_kv_ctx, &ctx);
}

ScRendered *sc_capture_columns(const ScColumns *columns) {
    CtxColumns ctx = { columns };
    return capture_render(render_columns_ctx, &ctx);
}

ScRendered *sc_capture_panel_str(const char *content, ScPanelOpts opts) {
    CtxPanelStr ctx = { content, opts };
    return capture_render(render_panel_str_ctx, &ctx);
}

ScRendered *sc_capture_panel_text(const ScText *content, ScPanelOpts opts) {
    CtxPanelText ctx = { content, opts };
    return capture_render(render_panel_text_ctx, &ctx);
}

ScRendered *sc_capture_rule_str(const char *title, ScRuleOpts opts) {
    CtxRuleStr ctx = { title, opts };
    return capture_render(render_rule_str_ctx, &ctx);
}

ScRendered *sc_capture_rule_text(const ScText *title, ScRuleOpts opts) {
    CtxRuleText ctx = { title, opts };
    return capture_render(render_rule_text_ctx, &ctx);
}


/* ── Stretch (panel filler) ──────────────────────────────────────────────── */

/**
 * Returns a heap-allocated empty content row matching the panel's vertical
 * border characters and background color. `inner_width` is the panel's
 * inner width (`max_column_width - 2`).
 */
static char *make_empty_panel_line(
    const ScPanelOpts *opts, int inner_width
) {
    const char *vertical = (opts->border.type != SC_BORDER_NONE
        && separator_chars[opts->border.type])
        ? separator_chars[opts->border.type] : " ";

    fflush(stdout);
    FILE *tmp = tmpfile();
    int saved_stdout = dup(STDOUT_FILENO);
    dup2(fileno(tmp), STDOUT_FILENO);

    sc_apply_colors(opts->border.color, opts->border.bg);
    fputs(vertical, stdout);
    fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);

    if (color_is_active(opts->bg)) {
        sc_apply_colors(SC_ANSI_COLOR_NONE, opts->bg);
        print_spaces(inner_width);
        fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
    } else {
        print_spaces(inner_width);
    }

    sc_apply_colors(opts->border.color, opts->border.bg);
    fputs(vertical, stdout);
    fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);

    fflush(stdout);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);

    fseek(tmp, 0, SEEK_END);
    long size = ftell(tmp);
    rewind(tmp);
    char *line = malloc((size_t)size + 1);
    fread(line, 1, (size_t)size, tmp);
    line[size] = '\0';
    fclose(tmp);
    return line;
}

/**
 * Returns a heap-allocated copy of `source` with `extra_lines` empty filler
 * lines inserted before the last line (so the bottom border stays at the
 * bottom). Caller owns the returned `ScRendered`.
 */
static ScRendered *stretch_rendered(
    const ScRendered *source, int extra_lines, const ScPanelOpts *opts
) {
    int inner_width = source->max_column_width - 2;
    if (inner_width < 0) { inner_width = 0; }
    char *empty_line = make_empty_panel_line(opts, inner_width);
    int empty_width = ansi_visible_width(empty_line);

    size_t new_count = source->line_count + (size_t)extra_lines;
    ScRendered *extended = malloc(sizeof(ScRendered));
    extended->lines = malloc(new_count * sizeof(char *));
    extended->column_widths = malloc(new_count * sizeof(int));
    extended->line_count = new_count;
    extended->max_column_width = source->max_column_width;

    size_t content_end = source->line_count - 1;
    for (size_t i = 0; i < content_end; i++) {
        extended->lines[i] = strdup(source->lines[i]);
        extended->column_widths[i] = source->column_widths[i];
    }
    for (int i = 0; i < extra_lines; i++) {
        size_t slot = content_end + (size_t)i;
        extended->lines[slot] = strdup(empty_line);
        extended->column_widths[slot] = empty_width;
    }
    extended->lines[new_count - 1] = strdup(source->lines[content_end]);
    extended->column_widths[new_count - 1] =
        source->column_widths[content_end];

    free(empty_line);
    return extended;
}


/* ── Print pipeline ──────────────────────────────────────────────────────── */

void sc_columns_print(const ScColumns *columns) {
    if (!columns || columns->count == 0) { return; }

    ColumnsRender self;
    init_render(&self, columns);

    print_newlines(columns->opts.margin.top);
    render_all_lines(&self);
    print_newlines(columns->opts.margin.bottom);

    cleanup_render(&self);
}

/** Builds the per-print render context. */
static void init_render(ColumnsRender *self, const ScColumns *columns) {
    self->columns = columns;
    self->separator = (columns->opts.sep.type > SC_BORDER_NONE)
        ? separator_chars[columns->opts.sep.type] : NULL;
    self->gap = columns->opts.gap > 0
        ? columns->opts.gap
        : (self->separator
            ? DEFAULT_GAP_WITH_SEPARATOR : DEFAULT_GAP_NO_SEPARATOR);
    self->left_margin = columns->opts.margin.left > 0
        ? columns->opts.margin.left : 0;

    size_t max_height = compute_max_height(columns);
    self->working = malloc(columns->count * sizeof(ScRendered *));
    for (size_t i = 0; i < columns->count; i++) {
        self->working[i] = columns->entries[i].rendered;
    }
    apply_stretch(self, max_height);

    self->column_widths = malloc(columns->count * sizeof(int));
    compute_column_widths(self);
    distribute_total_width(self);

    self->top_offsets = malloc(columns->count * sizeof(int));
    compute_vertical_offsets(self);

    self->total_height = 0;
    for (size_t i = 0; i < columns->count; i++) {
        if (self->working[i]->line_count > self->total_height) {
            self->total_height = self->working[i]->line_count;
        }
    }
}

/** Returns the maximum line count across all entries. */
static size_t compute_max_height(const ScColumns *columns) {
    size_t max_height = 0;
    for (size_t i = 0; i < columns->count; i++) {
        if (columns->entries[i].rendered->line_count > max_height) {
            max_height = columns->entries[i].rendered->line_count;
        }
    }
    return max_height;
}

/**
 * Replaces every working entry that is marked for `stretch` and shorter
 * than `max_height` with an extended copy that has filler rows inserted
 * before its bottom border.
 */
static void apply_stretch(ColumnsRender *self, size_t max_height) {
    for (size_t i = 0; i < self->columns->count; i++) {
        const ScColEntry *entry = &self->columns->entries[i];
        if (!entry->stretch
            || entry->rendered->line_count >= max_height) {
            continue;
        }
        int extra = (int)max_height - (int)entry->rendered->line_count;
        self->working[i] = stretch_rendered(
            entry->rendered, extra, &entry->panel_opts
        );
    }
}

/** Computes the rendered width of each column (natural, clamped, or fixed). */
static void compute_column_widths(ColumnsRender *self) {
    for (size_t i = 0; i < self->columns->count; i++) {
        const ScColItem *item = &self->columns->entries[i].item;
        int width = self->working[i]->max_column_width;
        if (item->fixed_w > 0) {
            width = item->fixed_w;
        } else {
            if (item->min_w > 0 && width < item->min_w) {
                width = item->min_w;
            }
            if (item->max_w > 0 && width > item->max_w) {
                width = item->max_w;
            }
        }
        self->column_widths[i] = width;
    }
}

/**
 * Distributes the layout's `total_width` across flex columns
 * (`fixed_w == 0`) by adding or subtracting a per-column delta to make the
 * widths plus gaps and separators sum to `total_width`.
 */
static void distribute_total_width(ColumnsRender *self) {
    const ScColumns *columns = self->columns;
    if (columns->opts.total_width <= 0) { return; }

    int used = 0;
    for (size_t i = 0; i < columns->count; i++) {
        used += self->column_widths[i];
    }
    int gap_and_sep_width = self->separator
        ? (self->gap * 2 + 1) : self->gap;
    for (size_t i = 0; i + 1 < columns->count; i++) {
        used += gap_and_sep_width;
    }
    int delta = columns->opts.total_width - used;
    if (delta == 0) { return; }

    int flex_count = 0;
    for (size_t i = 0; i < columns->count; i++) {
        if (columns->entries[i].item.fixed_w == 0) { flex_count++; }
    }
    if (flex_count == 0) { return; }

    int per_column = delta / flex_count;
    int remainder = delta - per_column * flex_count;
    int sign = remainder >= 0 ? 1 : -1;
    if (remainder < 0) { remainder = -remainder; }

    for (size_t i = 0; i < columns->count; i++) {
        if (columns->entries[i].item.fixed_w > 0) { continue; }
        int adjustment = per_column + (remainder > 0 ? sign : 0);
        if (remainder > 0) { remainder--; }
        int new_width = self->column_widths[i] + adjustment;
        if (new_width < 0) { new_width = 0; }
        self->column_widths[i] = new_width;
    }
}

/** Computes the top offset of each column from its vertical alignment. */
static void compute_vertical_offsets(ColumnsRender *self) {
    size_t total_height = 0;
    for (size_t i = 0; i < self->columns->count; i++) {
        if (self->working[i]->line_count > total_height) {
            total_height = self->working[i]->line_count;
        }
    }

    for (size_t i = 0; i < self->columns->count; i++) {
        const ScColItem *item = &self->columns->entries[i].item;
        ScVAlign valign = item->valign_set
            ? item->valign : self->columns->opts.valign;
        int height = (int)self->working[i]->line_count;
        self->top_offsets[i] = get_vertical_offset(
            valign, height, (int)total_height
        );
    }
}

/** Returns the top offset for `column_height` lines within `total_height`. */
static int get_vertical_offset(
    ScVAlign valign, int column_height, int total_height
) {
    int extra = total_height - column_height;
    if (valign == SC_VALIGN_MIDDLE) { return extra / 2; }
    if (valign == SC_VALIGN_BOTTOM) { return extra; }
    return 0;
}

/** Iterates all output rows and renders each one. */
static void render_all_lines(const ColumnsRender *self) {
    for (size_t line_index = 0; line_index < self->total_height; line_index++) {
        render_line(self, line_index);
    }
}

/** Renders a single output row (one screen line). */
static void render_line(const ColumnsRender *self, size_t line_index) {
    print_spaces(self->left_margin);
    for (size_t column_index = 0;
         column_index < self->columns->count;
         column_index++) {
        render_cell(self, column_index, line_index);
        bool is_last_column = column_index + 1 == self->columns->count;
        if (!is_last_column) {
            render_separator_or_gap(self);
        }
    }
    fputc('\n', stdout);
}

/**
 * Renders one cell at `(column_index, line_index)`: either the matching
 * content row or an empty filler when the column is shorter than the
 * current line index suggests.
 */
static void render_cell(
    const ColumnsRender *self, size_t column_index, size_t line_index
) {
    const ScColEntry *entry = &self->columns->entries[column_index];
    const ScRendered *rendered = self->working[column_index];
    int column_width = self->column_widths[column_index];

    int relative = get_relative_line_index(self, column_index, line_index);
    bool has_content = relative >= 0 && relative < (int)rendered->line_count;

    if (!has_content) {
        render_empty_cell(column_width, entry->item.bg);
        return;
    }

    render_content_cell(
        rendered->lines[relative],
        rendered->column_widths[relative],
        column_width,
        entry->item
    );
}

/**
 * Returns the row index inside the column's rendered widget for the given
 * output line, or `-1` when the line falls outside the column's content.
 */
static int get_relative_line_index(
    const ColumnsRender *self, size_t column_index, size_t line_index
) {
    return (int)line_index - self->top_offsets[column_index];
}

/**
 * Prints one content cell with left/right alignment padding, applying
 * `item.bg` to the padding (and re-applying it inside the line on every
 * `\033[0m` reset emitted by the captured content).
 */
static void render_content_cell(
    const char *line, int content_width, int column_width, ScColItem item
) {
    int spare = column_width - content_width;
    if (spare < 0) { spare = 0; }
    int left_pad = 0;
    int right_pad = spare;
    if (item.align == SC_ALIGN_CENTER) {
        left_pad = spare / 2;
        right_pad = spare - left_pad;
    } else if (item.align == SC_ALIGN_RIGHT) {
        left_pad = spare;
        right_pad = 0;
    }

    bool has_bg = color_is_active(item.bg);
    if (has_bg) {
        sc_apply_colors(SC_ANSI_COLOR_NONE, item.bg);
        print_spaces(left_pad);
        print_line_with_bg(line, item.bg);
        print_spaces(right_pad);
        fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
    } else {
        print_spaces(left_pad);
        fputs(line, stdout);
        print_spaces(right_pad);
    }
}

/**
 * Prints `line`, re-applying `bg` after every `\\033[0m` reset emitted by
 * the captured content so the column background does not get cancelled.
 */
static void print_line_with_bg(const char *line, ScColor bg) {
    const char *cursor = line;
    while (*cursor) {
        bool is_reset = cursor[0] == '\033' && cursor[1] == '['
            && cursor[2] == '0' && cursor[3] == 'm';
        if (is_reset) {
            fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
            cursor += 4;
            if (*cursor) { sc_apply_colors(SC_ANSI_COLOR_NONE, bg); }
        } else {
            fputc(*cursor++, stdout);
        }
    }
}

/** Renders an empty cell of `column_width` columns, optionally with `bg`. */
static void render_empty_cell(int column_width, ScColor bg) {
    if (color_is_active(bg)) {
        sc_apply_colors(SC_ANSI_COLOR_NONE, bg);
        print_spaces(column_width);
        fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
    } else {
        print_spaces(column_width);
    }
}

/** Dispatches to `render_separator` or `render_gap_spaces` per layout. */
static void render_separator_or_gap(const ColumnsRender *self) {
    if (self->separator) {
        render_separator(self, self->separator);
    } else {
        render_gap_spaces(self);
    }
}

/**
 * Prints `gap` spaces + separator + `gap` spaces, applying `sep.color` and
 * `sep.bg` per the layout options. The bg fills the gap spaces and the
 * separator char; the color only applies to the separator char.
 */
static void render_separator(
    const ColumnsRender *self, const char *separator
) {
    bool has_sep_bg = color_is_active(self->columns->opts.sep.bg);
    if (has_sep_bg) {
        sc_apply_colors(SC_ANSI_COLOR_NONE, self->columns->opts.sep.bg);
    }
    print_spaces(self->gap);

    if (color_is_active(self->columns->opts.sep.color)) {
        sc_apply_colors(
            self->columns->opts.sep.color, self->columns->opts.sep.bg
        );
        fputs(separator, stdout);
        if (has_sep_bg) {
            sc_apply_colors(SC_ANSI_COLOR_NONE, self->columns->opts.sep.bg);
        } else {
            fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
        }
    } else {
        fputs(separator, stdout);
    }

    print_spaces(self->gap);
    if (has_sep_bg) { fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout); }
}

/** Prints `gap` spaces between columns when no separator is configured. */
static void render_gap_spaces(const ColumnsRender *self) {
    bool has_sep_bg = color_is_active(self->columns->opts.sep.bg);
    if (has_sep_bg) {
        sc_apply_colors(SC_ANSI_COLOR_NONE, self->columns->opts.sep.bg);
    }
    print_spaces(self->gap);
    if (has_sep_bg) { fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout); }
}

/** Frees stretched copies and per-print buffers. */
static void cleanup_render(ColumnsRender *self) {
    for (size_t i = 0; i < self->columns->count; i++) {
        if (self->working[i] != self->columns->entries[i].rendered) {
            sc_rendered_free(self->working[i]);
        }
    }
    free(self->working);
    free(self->column_widths);
    free(self->top_offsets);
}
