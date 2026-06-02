#include "sparcli.h"
#include "core/text_internal.h"
#include "internal.h"
#include "core/render_wrap.h"
#include "output/table/table_internal.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


// Forward declarations indented to reflect call hierarchy
static void scan_text_into_builder(
    const char *text, ScTextStyle style, LineBuilder *builder
);
static void resolve_cell_span(
    const ScCell *cell, size_t span_index,
    const char **out_text, ScTextStyle *out_style
);


/* ── make_cell_lines ────────────────────────────────────────────────────── */

/**
 * Splits `cell` content on `\n` into an array of `ScRenderLine` values.
 *
 * Each `ScRenderLine` owns heap copies of its span strings; free with
 * `free_tlines`.
 *
 * @param cell       Source cell.
 * @param out_count  Receives the number of returned lines.
 * @return           Heap-allocated `ScRenderLine` array (length `*out_count`).
 */
ScRenderLine *make_cell_lines(
    const ScCell *cell, ScAnsiMode ansi, size_t *out_count
) {
    LineBuilder builder;
    lb_init(&builder);

    bool is_rich = (cell->kind == SC_CELL_TEXT
                    || cell->kind == SC_CELL_MARKUP) && cell->text;
    size_t span_count = is_rich ? cell->text->count : 1;

    for (size_t span_index = 0; span_index < span_count; span_index++) {
        const char *text;
        ScTextStyle style;
        resolve_cell_span(cell, span_index, &text, &style);
        // Borrowed SC_CELL_STR strings cross the trust boundary here;
        // rich-text spans were already sanitized at append time.
        char *clean = is_rich ? NULL : sc_sanitize_copy_mode(text, ansi);
        scan_text_into_builder(clean ? clean : text, style, &builder);
        free(clean);
    }
    lb_finalize_line(&builder);
    lb_release_span_buffer(&builder);

    *out_count = builder.line_count;
    return builder.lines;
}

/**
 * Returns the visible width of the widest line in `cell`.
 */
size_t cell_visible_width(const ScCell *cell) {
    size_t line_count;
    // Measurement is ANSI-mode independent (widths skip escape sequences
    // and dropped control bytes), so no stripping is needed here.
    ScRenderLine *lines =
        make_cell_lines(cell, SC_ANSI_MODE_ALLOW, &line_count);
    size_t max_width = 0;
    for (size_t i = 0; i < line_count; i++) {
        if (lines[i].visible_width > max_width) {
            max_width = lines[i].visible_width;
        }
    }
    free_tlines(lines, line_count);
    return max_width;
}

/**
 * Builds the cell's lines just to count them and immediately frees them.
 * Honors word-wrap when enabled and `available_width > 0`.
 */
size_t count_cell_lines(
    const ScCell *cell, const ScColOpts *col_opts, int available_width
) {
    size_t line_count;
    ScRenderLine *lines = (col_opts->word_wrap && available_width > 0)
        ? wrap_cell_lines(cell, SC_ANSI_MODE_ALLOW, available_width,
                          &line_count)
        : make_cell_lines(cell, SC_ANSI_MODE_ALLOW, &line_count);
    free_tlines(lines, line_count);
    return line_count;
}

/** Frees every span string, then the spans array and the lines array. */
void free_tlines(ScRenderLine *lines, size_t line_count) {
    sc_free_render_lines(lines, line_count);
}


/* ── wrap_cell_lines ────────────────────────────────────────────────────── */

/**
 * Returns word-wrapped copies of every line in `cell` longer than
 * `max_width`; lines that already fit are deep-copied unchanged.
 */
ScRenderLine *wrap_cell_lines(
    const ScCell *cell, ScAnsiMode ansi, int max_width, size_t *out_count
) {
    size_t raw_count;
    ScRenderLine *raw_lines = make_cell_lines(cell, ansi, &raw_count);
    if (max_width <= 0) {
        *out_count = raw_count;
        return raw_lines;
    }

    ScRenderLine *wrapped = sc_wrap_render_lines(
        raw_lines, raw_count, max_width, out_count
    );
    free_tlines(raw_lines, raw_count);
    return wrapped;
}


/* ── Newline scanner ────────────────────────────────────────────────────── */

/**
 * Scans `text` for `\n` boundaries, pushing each segment into the builder
 * via `lb_push_span` and finalizing a line on every newline. Any tail
 * after the last newline stays in the buffer for the caller to flush.
 */
static void scan_text_into_builder(
    const char *text, ScTextStyle style, LineBuilder *builder
) {
    const char *segment_start = text;
    const char *cursor = text;
    while (*cursor) {
        if (*cursor == '\n') {
            size_t length = (size_t)(cursor - segment_start);
            if (length > 0) {
                size_t visible_width = sc_utf8_string_length(
                    segment_start, length
                );
                lb_push_span(
                    builder, segment_start, length, visible_width, style
                );
            }
            lb_finalize_line(builder);
            segment_start = cursor + 1;
        }
        cursor++;
    }
    size_t tail_length = (size_t)(cursor - segment_start);
    if (tail_length > 0) {
        size_t visible_width = sc_utf8_string_length(
            segment_start, tail_length
        );
        lb_push_span(
            builder, segment_start, tail_length, visible_width, style
        );
    }
}

/**
 * Resolves the raw text and style for span `span_index` of `cell`.
 * `SC_CELL_STR` cells always use a single unstyled span.
 */
static void resolve_cell_span(
    const ScCell *cell, size_t span_index,
    const char **out_text, ScTextStyle *out_style
) {
    static const ScTextStyle plain = {
        SC_TEXT_ATTR_NONE,
        SC_ANSI_COLOR_NONE,
        SC_ANSI_COLOR_NONE,
    };

    if (cell->kind == SC_CELL_STR || !cell->text) {
        // SC_CELL_STR, or a rich cell built with a NULL ScText (e.g.
        // `sc_cell_t(NULL)`): fall back to a single empty unstyled span
        // instead of dereferencing a NULL `text`.
        *out_text = (cell->kind == SC_CELL_STR && cell->str) ? cell->str : "";
        *out_style = plain;
        return;
    }
    *out_text = cell->text->spans[span_index].raw_str;
    *out_style = cell->text->spans[span_index].style;
}
