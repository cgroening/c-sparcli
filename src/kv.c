#include "sparcli.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** Default separator between key and value (two spaces). */
#define DEFAULT_SEPARATOR "  "

/** Minimum width preserved for the value column on narrow terminals. */
#define MIN_VALUE_WIDTH 1

/** Buffer size used when emitting a slice of text through `sc_print`. */
#define PRINT_SLICE_BUFFER 512

/** Word-wrap line buffer (covers any reasonable terminal width). */
#define WRAP_LINE_BUFFER 4096


/** One key-value pair. */
typedef struct ScKVEntry {
    /** Heap-allocated key string. */
    char *key;

    /** Heap-allocated value string. */
    char *value;
} ScKVEntry;

/** Key-value list container. */
struct ScKV {
    /** Heap-allocated array of entries. */
    ScKVEntry *entries;

    /** Number of valid entries. */
    size_t entry_count;

    /** Allocated capacity of `entries`. */
    size_t entry_capacity;

    /** Rendering options captured at construction. */
    ScKVOpts opts;
};

/** Render-time context for a single print call. */
typedef struct KV {
    /** List being rendered; not owned. */
    const ScKV *kv;

    /** Left outer margin in columns. */
    int left_margin;

    /** Right outer margin in columns. */
    int right_margin;

    /** Total line width in columns. */
    int total_width;

    /** Resolved key column width. */
    int key_column_width;

    /** Resolved separator string (never `NULL`). */
    const char *separator;

    /** Visible width of `separator`. */
    int separator_width;

    /** Width in columns available for the value field. */
    int value_width;

    /**
     * Column at which continuation lines of a wrapped value start
     * (left margin + key column + separator).
     */
    int continuation_indent;

    /** `true` when `opts.key_style` carries formatting. */
    bool key_has_format;

    /** `true` when `opts.val_style` carries formatting. */
    bool val_has_format;
} KV;


// Forward declarations indented to reflect call hierarchy
static void append_entry(ScKV *kv, const char *key, const char *value);

static void init_kv(KV *self, const ScKV *kv);
    static int get_horizontal_margin(int configured);
    static int get_total_width(const ScKVOpts *opts);
    static int get_key_column_width(const ScKV *kv, int configured);
    static int get_value_width(const KV *self);

static void render_entry(const KV *self, size_t entry_index);
    static void render_key(const KV *self, const char *key);
        static void print_text_slice(
            const char *text, int byte_count, ScTextStyle style, bool styled
        );
    static void render_value(const KV *self, const char *value);
        static void render_wrapped_value(const KV *self, const char *value);
        static void render_truncated_value(const KV *self, const char *value);



ScKV *sc_kv_new(ScKVOpts opts) {
    ScKV *kv = calloc(1, sizeof(ScKV));
    if (!kv) { return NULL; }
    kv->opts = opts;
    return kv;
}

void sc_kv_add(ScKV *kv, const char *key, const char *value) {
    if (!kv) { return; }
    append_entry(kv, key, value);
}

void sc_kv_print(const ScKV *kv) {
    if (!kv || kv->entry_count == 0) { return; }

    KV self;
    init_kv(&self, kv);

    sc_print_newlines(kv->opts.margin.top);
    for (size_t i = 0; i < kv->entry_count; i++) {
        render_entry(&self, i);
    }
    sc_print_newlines(kv->opts.margin.bottom);
}

void sc_kv_free(ScKV *kv) {
    if (!kv) { return; }
    for (size_t i = 0; i < kv->entry_count; i++) {
        free(kv->entries[i].key);
        free(kv->entries[i].value);
    }
    free(kv->entries);
    free(kv);
}


/** Appends an entry to `kv`, growing the array as needed. */
static void append_entry(ScKV *kv, const char *key, const char *value) {
    if (kv->entry_count == kv->entry_capacity) {
        kv->entry_capacity = kv->entry_capacity ? kv->entry_capacity * 2 : 8;
        kv->entries = realloc(
            kv->entries, kv->entry_capacity * sizeof(ScKVEntry)
        );
    }
    kv->entries[kv->entry_count].key = strdup(key ? key : "");
    kv->entries[kv->entry_count].value = strdup(value ? value : "");
    kv->entry_count++;
}


/** Populates `self` with all derived layout values for one print call. */
static void init_kv(KV *self, const ScKV *kv) {
    self->kv = kv;
    self->left_margin = get_horizontal_margin(kv->opts.margin.left);
    self->right_margin = get_horizontal_margin(kv->opts.margin.right);
    self->total_width = get_total_width(&kv->opts);
    self->key_column_width = get_key_column_width(kv, kv->opts.key_width);

    self->separator = kv->opts.sep ? kv->opts.sep : DEFAULT_SEPARATOR;
    self->separator_width = (int)sc_utf8_string_length(
        self->separator, strlen(self->separator)
    );

    self->value_width = get_value_width(self);
    self->continuation_indent = self->left_margin
        + self->key_column_width + self->separator_width;

    self->key_has_format = sc_style_has_format(kv->opts.key_style);
    self->val_has_format = sc_style_has_format(kv->opts.val_style);
}

/** Returns `configured` clamped to `>= 0`. */
static int get_horizontal_margin(int configured) {
    return configured > 0 ? configured : 0;
}

/** Returns `opts.width` when set, otherwise the current terminal width. */
static int get_total_width(const ScKVOpts *opts) {
    return opts->width > 0 ? opts->width : sc_terminal_width();
}

/**
 * Returns the configured key column width, or the visible width of the
 * widest key when `configured <= 0`.
 */
static int get_key_column_width(const ScKV *kv, int configured) {
    if (configured > 0) { return configured; }

    int widest = 0;
    for (size_t i = 0; i < kv->entry_count; i++) {
        const char *key = kv->entries[i].key;
        int width = (int)sc_utf8_string_length(key, strlen(key));
        if (width > widest) { widest = width; }
    }
    return widest;
}

/**
 * Returns the value-field width: total width minus margins, key column and
 * separator. Clamped to at least `MIN_VALUE_WIDTH`.
 */
static int get_value_width(const KV *self) {
    int available = self->total_width - self->left_margin - self->right_margin
                    - self->key_column_width - self->separator_width;
    return available < MIN_VALUE_WIDTH ? MIN_VALUE_WIDTH : available;
}

/**
 * Returns `true` when `style` carries any formatting; zero-initialized
 * `ScTextStyle` returns `false`.
 */


/** Renders one entry and the trailing item gap (when not the last entry). */
static void render_entry(const KV *self, size_t entry_index) {
    const ScKVEntry *entry = &self->kv->entries[entry_index];

    sc_print_spaces(self->left_margin);
    render_key(self, entry->key);
    fputs(self->separator, sc_output_stream());
    render_value(self, entry->value);

    bool is_last = entry_index + 1 == self->kv->entry_count;
    if (!is_last) {
        sc_print_newlines(self->kv->opts.item_gap);
    }
}


/**
 * Prints `key` padded or truncated to `self->key_column_width`, applying
 * `opts.key_style` when it carries formatting.
 */
static void render_key(const KV *self, const char *key) {
    int field_width = self->key_column_width;
    int natural_width = (int)sc_utf8_string_length(key, strlen(key));
    int print_byte_count = (natural_width > field_width)
        ? (int)sc_utf8_trim_to_cols(key, field_width)
        : (int)strlen(key);
    int printed_width = natural_width < field_width
        ? natural_width : field_width;

    print_text_slice(
        key, print_byte_count,
        self->kv->opts.key_style, self->key_has_format
    );
    sc_print_spaces(field_width - printed_width);
}

/**
 * Prints the first `byte_count` bytes of `text`; applies `style` through
 * `sc_print` when `styled` is `true`, otherwise writes the slice directly.
 */
static void print_text_slice(
    const char *text, int byte_count, ScTextStyle style, bool styled
) {
    if (!styled) {
        fwrite(text, 1, (size_t)byte_count, sc_output_stream());
        return;
    }
    if (byte_count >= PRINT_SLICE_BUFFER) { return; }

    char buffer[PRINT_SLICE_BUFFER];
    memcpy(buffer, text, (size_t)byte_count);
    buffer[byte_count] = '\0';
    sc_print(buffer, style);
}

/** Dispatches to the wrapped or truncated value renderer. */
static void render_value(const KV *self, const char *value) {
    if (self->kv->opts.wrap_val) {
        render_wrapped_value(self, value);
    } else {
        render_truncated_value(self, value);
    }
}

/**
 * Word-wraps `value` into lines of at most `self->value_width` columns and
 * prints each line; continuation lines are indented to align under the
 * value column.
 */
static void render_wrapped_value(const KV *self, const char *value) {
    if (!value || !*value) {
        fputc('\n', sc_output_stream());
        return;
    }

    const char *cursor = value;
    bool is_first_line = true;

    while (*cursor) {
        char line_buffer[WRAP_LINE_BUFFER];
        int byte_count = 0;
        int visible_count = 0;

        while (*cursor) {
            const char *word_end = cursor;
            while (*word_end && *word_end != ' ') { word_end++; }
            int word_bytes = (int)(word_end - cursor);
            int word_width = (int)sc_utf8_string_length(
                cursor, (size_t)word_bytes
            );
            int gap = (visible_count > 0) ? 1 : 0;

            bool fits = visible_count + gap + word_width <= self->value_width;
            if (gap > 0 && !fits) { break; }

            if (gap) { line_buffer[byte_count++] = ' '; }
            memcpy(line_buffer + byte_count, cursor, (size_t)word_bytes);
            byte_count += word_bytes;
            visible_count += gap + word_width;

            cursor = word_end;
            while (*cursor == ' ') { cursor++; }
        }

        line_buffer[byte_count] = '\0';
        if (!is_first_line) { sc_print_spaces(self->continuation_indent); }
        is_first_line = false;

        if (self->val_has_format) {
            sc_print(line_buffer, self->kv->opts.val_style);
        } else {
            fputs(line_buffer, sc_output_stream());
        }
        fputc('\n', sc_output_stream());
    }
}

/**
 * Prints `value` truncated to `self->value_width` columns; no continuation
 * lines.
 */
static void render_truncated_value(const KV *self, const char *value) {
    int byte_count = (int)sc_utf8_trim_to_cols(value, self->value_width);
    print_text_slice(
        value, byte_count,
        self->kv->opts.val_style, self->val_has_format
    );
    fputc('\n', sc_output_stream());
}


