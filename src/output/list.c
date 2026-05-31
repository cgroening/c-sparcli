#include "sparcli.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** Default bullet character (`•`, U+2022). */
#define DEFAULT_BULLET "\xe2\x80\xa2"

/** Default suffix after a numbered/alpha/roman marker value. */
#define DEFAULT_MARKER_SUFFIX "."

/** Default prefix before a numbered/alpha/roman marker value. */
#define DEFAULT_MARKER_PREFIX ""

/** Buffer size used to format a single marker value (number, roman, …). */
#define MARKER_VALUE_BUFFER 64

/** Buffer size used to assemble the full marker token (prefix + value + …). */
#define MARKER_FIELD_BUFFER 256

/** Minimum text width preserved for an item even on narrow terminals. */
#define MIN_AVAILABLE_TEXT_WIDTH 4


/** One item in a list. */
struct ScListItem {
    /** `true` = `text` is used as content; `false` = `str` is used. */
    bool is_text;

    /** Plain-string content; owned copy when set; `NULL` for text items. */
    char *str;

    /** Rich-text content; not owned; `NULL` for string items. */
    const ScText *text;

    /** Style applied to `str`; ignored for text items. */
    ScTextStyle style;

    /** Optional sub-list; owned by the item. */
    struct ScList *sublist;
};

/** List container. */
struct ScList {
    /** Heap-allocated array of item pointers; owned. */
    struct ScListItem **items;

    /** Number of valid entries in `items`. */
    size_t item_count;

    /** Allocated capacity of `items`. */
    size_t item_capacity;

    /** Rendering options provided at construction. */
    ScListOpts opts;
};

/** Render-time context for one level of a list. */
typedef struct List {
    /** List being rendered at this level; not owned. */
    const ScList *list;

    /** Total line width in columns. */
    int terminal_width;

    /** Indent inherited from the parent item; `0` for the outer list. */
    int base_indent;

    /** Column at which the marker field starts (base + opts.indent). */
    int list_indent;

    /** Width in columns of the full marker field. */
    int marker_field_width;

    /** Width in columns of the widest marker value in this list. */
    int max_marker_value_width;

    /** Column at which item content (and continuation lines) starts. */
    int text_start_column;

    /** Width in columns available for item content. */
    int available_text_width;

    /** `true` when this list uses `SC_LIST_BULLET`. */
    bool marker_is_bullet;

    /** Resolved marker prefix string (never `NULL`). */
    const char *marker_prefix;

    /** Resolved marker suffix string (never `NULL`). */
    const char *marker_suffix;
} List;


// Forward declarations indented to reflect call hierarchy
static ScListItem *new_item(
    bool is_text, const char *str, ScTextStyle style, const ScText *text
);
static void append_item(ScList *list, ScListItem *item);

static void render_list_level(
    const ScList *list, int base_indent, int terminal_width
);
    static void init_list(
        List *self, const ScList *list, int base_indent, int terminal_width
    );
        static int get_marker_field_width(
            const ScList *list, int max_marker_value_width
        );
            static const char *get_bullet_char(const ScListOpts *opts);
        static int get_max_marker_value_width(const ScList *list);
            static int marker_value_visible_width(const ScList *list, int index);
                static void format_marker_value(
                    const ScList *list, int index, char *buffer
                );
                    static void to_roman(
                        int number, char *buffer, bool uppercase
                    );
        static int get_available_text_width(
            const ScListOpts *opts, int terminal_width, int text_start_column
        );
    static void render_item(List *self, size_t item_index);
        static void render_left_indent(const List *self);
        static void render_marker(const List *self, size_t item_index);
            static void build_marker_string(
                const List *self, size_t item_index,
                char *buffer, size_t buffer_size
            );
        static void render_item_content(
            const List *self, const ScListItem *item
        );
            static void render_wrapped_string(
                const List *self, const ScListItem *item
            );
                static char **word_wrap(
                    const char *text, int max_width, size_t *out_line_count
                );

static void item_free(ScListItem *item);


ScList *sc_list_new(ScListOpts opts) {
    ScList *list = calloc(1, sizeof(ScList));
    list->opts = opts;
    return list;
}

ScListItem *sc_list_add_str(
    ScList *list, const char *str, ScTextStyle style
) {
    if (!list) { return NULL; }
    ScListItem *item = new_item(false, str, style, NULL);
    append_item(list, item);
    return item;
}

ScListItem *sc_list_add_text(ScList *list, const ScText *text) {
    if (!list) { return NULL; }
    ScTextStyle no_style = {
        SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
    };
    ScListItem *item = new_item(true, NULL, no_style, text);
    append_item(list, item);
    return item;
}

ScList *sc_list_add_sub(ScListItem *parent, ScListOpts opts) {
    if (!parent) { return NULL; }
    sc_list_free(parent->sublist);
    if (!parent) { return NULL; }
    parent->sublist = sc_list_new(opts);
    return parent->sublist;
}

void sc_list_print(const ScList *list) {
    if (!list) { return; }
    sc_print_newlines(list->opts.margin.top);
    int terminal_width = list->opts.width > 0
        ? list->opts.width : sc_terminal_width();
    render_list_level(list, 0, terminal_width);
    sc_print_newlines(list->opts.margin.bottom);
}

void sc_list_free(ScList *list) {
    if (!list) { return; }
    for (size_t i = 0; i < list->item_count; i++) {
        item_free(list->items[i]);
    }
    free(list->items);
    free(list);
}


/** Allocates a new item and copies the string field when present. */
static ScListItem *new_item(
    bool is_text, const char *str, ScTextStyle style, const ScText *text
) {
    ScListItem *item = calloc(1, sizeof(ScListItem));
    item->is_text = is_text;
    item->str = str ? strdup(str) : NULL;
    item->style = style;
    item->text = text;
    return item;
}

/** Appends `item` to `list`, growing the items array as needed. */
static void append_item(ScList *list, ScListItem *item) {
    if (list->item_count == list->item_capacity) {
        void *grown = sc_dynarray_grow(
            list->items, &list->item_capacity, sizeof(ScListItem *), 4
        );
        // On OOM leave `item` unappended. We don't free it: its pointer is
        // also returned to the caller, so freeing here would dangle that.
        if (!grown) { return; }
        list->items = grown;
    }
    list->items[list->item_count++] = item;
}


/** Renders one level of a list; recurses into sub-lists via `render_item`. */
static void render_list_level(
    const ScList *list, int base_indent, int terminal_width
) {
    if (!list || list->item_count == 0) { return; }

    List self;
    init_list(&self, list, base_indent, terminal_width);

    for (size_t i = 0; i < list->item_count; i++) {
        render_item(&self, i);
    }
}

/** Populates `self` with all derived layout values for one list level. */
static void init_list(
    List *self, const ScList *list, int base_indent, int terminal_width
) {
    self->list = list;
    self->base_indent = base_indent;
    self->terminal_width = terminal_width;
    self->marker_is_bullet = (list->opts.marker == SC_LIST_BULLET);
    self->marker_prefix = list->opts.marker_prefix
        ? list->opts.marker_prefix : DEFAULT_MARKER_PREFIX;
    self->marker_suffix = list->opts.marker_suffix
        ? list->opts.marker_suffix : DEFAULT_MARKER_SUFFIX;

    self->list_indent = base_indent + list->opts.indent;
    self->max_marker_value_width = get_max_marker_value_width(list);
    self->marker_field_width = get_marker_field_width(
        list, self->max_marker_value_width
    );
    self->text_start_column = self->list_indent + self->marker_field_width;
    self->available_text_width = get_available_text_width(
        &list->opts, terminal_width, self->text_start_column
    );
}

/**
 * Returns the full marker field width: bullet + space, or
 * prefix + value-field + suffix + space.
 */
static int get_marker_field_width(
    const ScList *list, int max_marker_value_width
) {
    if (list->opts.marker == SC_LIST_BULLET) {
        const char *bullet = get_bullet_char(&list->opts);
        return (int)sc_utf8_string_length(bullet, strlen(bullet)) + 1;
    }
    const char *prefix = list->opts.marker_prefix
        ? list->opts.marker_prefix : DEFAULT_MARKER_PREFIX;
    const char *suffix = list->opts.marker_suffix
        ? list->opts.marker_suffix : DEFAULT_MARKER_SUFFIX;
    int prefix_width = (int)sc_utf8_string_length(prefix, strlen(prefix));
    int suffix_width = (int)sc_utf8_string_length(suffix, strlen(suffix));
    return prefix_width + max_marker_value_width + suffix_width + 1;
}

/** Returns the configured bullet character or the default `•`. */
static const char *get_bullet_char(const ScListOpts *opts) {
    return opts->bullet ? opts->bullet : DEFAULT_BULLET;
}

/**
 * Returns the visible width of the widest marker value in `list`, or the
 * bullet width for bullet lists. Always `>= 1`.
 */
static int get_max_marker_value_width(const ScList *list) {
    if (list->opts.marker == SC_LIST_BULLET) {
        const char *bullet = get_bullet_char(&list->opts);
        return (int)sc_utf8_string_length(bullet, strlen(bullet));
    }
    int max_width = 0;
    for (size_t i = 0; i < list->item_count; i++) {
        int width = marker_value_visible_width(list, (int)i);
        if (width > max_width) { max_width = width; }
    }
    return max_width > 0 ? max_width : 1;
}

/**
 * Returns the visible width of the marker value at `index` (1-based for
 * the user, 0-based here).
 */
static int marker_value_visible_width(const ScList *list, int index) {
    char buffer[MARKER_VALUE_BUFFER];
    format_marker_value(list, index, buffer);
    return (int)sc_utf8_string_length(buffer, strlen(buffer));
}

/**
 * Writes the bare marker value (no prefix/suffix) for the item at `index`
 * into `buffer`; `buffer` must be at least `MARKER_VALUE_BUFFER` bytes long.
 */
static void format_marker_value(
    const ScList *list, int index, char *buffer
) {
    int number = index + 1;
    switch (list->opts.marker) {
    case SC_LIST_BULLET:
        snprintf(buffer, MARKER_VALUE_BUFFER, "%s",
                 get_bullet_char(&list->opts));
        break;
    case SC_LIST_NUMBER:
        snprintf(buffer, MARKER_VALUE_BUFFER, "%d", number);
        break;
    case SC_LIST_ALPHA_LC:
        buffer[0] = (char)('a' + (number - 1) % 26);
        buffer[1] = '\0';
        break;
    case SC_LIST_ALPHA_UC:
        buffer[0] = (char)('A' + (number - 1) % 26);
        buffer[1] = '\0';
        break;
    case SC_LIST_ROMAN_LC:
        to_roman(number, buffer, false);
        break;
    case SC_LIST_ROMAN_UC:
        to_roman(number, buffer, true);
        break;
    }
}

/**
 * Writes the Roman-numeral representation of `number` into `buffer`,
 * using uppercase letters when `uppercase` is `true`. `buffer` must hold
 * at least `MARKER_VALUE_BUFFER` bytes (enough for any practical roman
 * numeral; the longest cluster within `int` range is < 20 chars).
 *
 * Uses position-tracked `snprintf` so we never run past the buffer end –
 * unlike the previous `strcpy`/`strcat` version which silently relied on
 * the caller-provided buffer being large enough.
 */
static void to_roman(int number, char *buffer, bool uppercase) {
    static const struct {
        int value;
        const char *upper;
        const char *lower;
    } table[] = {
        { 1000, "M",  "m"  }, { 900, "CM", "cm" },
        {  500, "D",  "d"  }, { 400, "CD", "cd" },
        {  100, "C",  "c"  }, {  90, "XC", "xc" },
        {   50, "L",  "l"  }, {  40, "XL", "xl" },
        {   10, "X",  "x"  }, {   9, "IX", "ix" },
        {    5, "V",  "v"  }, {   4, "IV", "iv" },
        {    1, "I",  "i"  }, {   0, NULL, NULL },
    };

    if (number <= 0) {
        snprintf(buffer, MARKER_VALUE_BUFFER, "0");
        return;
    }

    size_t position = 0;
    for (int i = 0; table[i].value > 0; i++) {
        const char *literal = uppercase ? table[i].upper : table[i].lower;
        while (number >= table[i].value) {
            int written = snprintf(
                buffer + position, MARKER_VALUE_BUFFER - position,
                "%s", literal
            );
            if (written < 0
                || (size_t)written >= MARKER_VALUE_BUFFER - position) {
                /* Would overflow the buffer – stop gracefully. */
                return;
            }
            position += (size_t)written;
            number -= table[i].value;
        }
    }
}

/**
 * Returns the width in columns available for item content; clamped to at
 * least `MIN_AVAILABLE_TEXT_WIDTH`.
 */
static int get_available_text_width(
    const ScListOpts *opts, int terminal_width, int text_start_column
) {
    int available = terminal_width - opts->margin.left
                    - opts->margin.right - text_start_column;
    return available < MIN_AVAILABLE_TEXT_WIDTH
        ? MIN_AVAILABLE_TEXT_WIDTH : available;
}


/** Renders the item at `item_index`, its content and any attached sub-list. */
static void render_item(List *self, size_t item_index) {
    const ScListItem *item = self->list->items[item_index];

    render_left_indent(self);
    render_marker(self, item_index);
    render_item_content(self, item);

    if (item->sublist) {
        render_list_level(
            item->sublist, self->text_start_column, self->terminal_width
        );
    }

    sc_print_newlines(self->list->opts.item_gap);
}

/** Prints the left margin plus the list indent for the current item. */
static void render_left_indent(const List *self) {
    sc_print_spaces(self->list->opts.margin.left + self->list_indent);
}


/**
 * Builds and prints the marker token, applying `marker_style` when it carries
 * formatting; otherwise emits the marker without ANSI codes.
 */
static void render_marker(const List *self, size_t item_index) {
    char marker_buffer[MARKER_FIELD_BUFFER];
    build_marker_string(
        self, item_index, marker_buffer, sizeof(marker_buffer)
    );

    if (sc_style_has_format(self->list->opts.marker_style)) {
        sc_print(marker_buffer, self->list->opts.marker_style);
    } else {
        fputs(marker_buffer, sc_output_stream());
    }
}

/**
 * Composes the marker token for the item at `item_index` into `buffer`:
 * bullet form `"<bullet> "` or counted form `"<prefix><pad><value><suffix> "`.
 */
static void build_marker_string(
    const List *self, size_t item_index, char *buffer, size_t buffer_size
) {
    char value_buffer[MARKER_VALUE_BUFFER];
    format_marker_value(self->list, (int)item_index, value_buffer);

    if (self->marker_is_bullet) {
        snprintf(buffer, buffer_size, "%s ", value_buffer);
        return;
    }

    int value_width = (int)sc_utf8_string_length(
        value_buffer, strlen(value_buffer)
    );
    int right_align_pad = self->max_marker_value_width - value_width;
    snprintf(
        buffer, buffer_size, "%s%*s%s%s ",
        self->marker_prefix, right_align_pad, "",
        value_buffer, self->marker_suffix
    );
}

/**
 * Returns `true` when `style` carries any formatting; zero-initialized
 * `ScTextStyle` returns `false` so callers can skip ANSI emission.
 */

/** Renders the item content (rich text or word-wrapped string). */
static void render_item_content(
    const List *self, const ScListItem *item
) {
    if (item->is_text) {
        if (item->text) { sc_print_text(item->text); }
        fputc('\n', sc_output_stream());
        return;
    }
    render_wrapped_string(self, item);
}

/**
 * Word-wraps `item->str` to the available text width and prints every line
 * with a hanging indent for continuations.
 */
static void render_wrapped_string(
    const List *self, const ScListItem *item
) {
    const char *text = item->str ? item->str : "";
    size_t line_count;
    char **lines = word_wrap(text, self->available_text_width, &line_count);

    for (size_t i = 0; i < line_count; i++) {
        if (i > 0) {
            sc_print_spaces(
                self->list->opts.margin.left + self->text_start_column
            );
        }
        sc_print(lines[i], item->style);
        fputc('\n', sc_output_stream());
        free(lines[i]);
    }
    free(lines);
}

/**
 * Word-wraps `text` to at most `max_width` visible columns per line.
 *
 * Breaks on spaces when one fits; otherwise produces a hard break at
 * `max_width`. The returned array and its strings must be freed by the
 * caller. On entry `*out_line_count` is ignored; on exit it holds the
 * number of returned lines.
 */
static char **word_wrap(
    const char *text, int max_width, size_t *out_line_count
) {
    size_t capacity = 8;
    size_t line_count = 0;
    char **lines = malloc(capacity * sizeof(char *));

    if (!text || !*text || max_width < 1) {
        lines[line_count++] = strdup(text && *text ? text : "");
        *out_line_count = line_count;
        return lines;
    }

    const char *cursor = text;
    while (*cursor) {
        while (*cursor == ' ') { cursor++; }
        if (!*cursor) { break; }

        const char *line_start = cursor;
        const char *last_space = NULL;
        const char *after_last_space = NULL;
        int visible_columns = 0;

        while (*cursor && *cursor != '\n') {
            unsigned char byte = *(unsigned char *)cursor;
            int byte_count = (byte < 0x80) ? 1
                : (byte < 0xE0) ? 2
                : (byte < 0xF0) ? 3 : 4;
            if (visible_columns + 1 > max_width) { break; }
            if (*cursor == ' ') {
                last_space = cursor;
                after_last_space = cursor + 1;
            }
            visible_columns++;
            cursor += byte_count;
        }

        const char *line_end;
        if (!*cursor || *cursor == '\n') {
            line_end = cursor;
            if (*cursor == '\n') { cursor++; }
        } else if (last_space) {
            line_end = last_space;
            cursor = after_last_space;
        } else {
            line_end = cursor;
        }

        while (line_end > line_start && *(line_end - 1) == ' ') {
            line_end--;
        }

        if (line_count == capacity) {
            capacity *= 2;
            lines = realloc(lines, capacity * sizeof(char *));
        }
        lines[line_count++] = strndup(
            line_start, (size_t)(line_end - line_start)
        );
    }

    if (line_count == 0) { lines[line_count++] = strdup(""); }
    *out_line_count = line_count;
    return lines;
}



/** Recursively frees `item`, its owned string and any sub-list. */
static void item_free(ScListItem *item) {
    if (!item) { return; }
    free(item->str);
    sc_list_free(item->sublist);
    free(item);
}
