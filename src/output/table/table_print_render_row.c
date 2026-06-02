#include "sparcli.h"
#include "internal.h"
#include "output/table/table_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/**
 * Per-row render context. Built once at the top of `render_row` and passed
 * to every helper as `Row *self`.
 */
typedef struct Row {
    /** Outer render context (column widths, padding, opts, …). */
    Table *table;

    /** Cells of the row being rendered. */
    const ScCell *cells;

    /** Resolved background color for the row. */
    ScColor row_bg;

    /** Section this row belongs to (data / header / footer). */
    RowSection section;

    /** Total visual height of the row in terminal lines. */
    int row_height;

    /** Per-column ScRenderLine arrays; `NULL` for skip/continuation columns. */
    ScRenderLine **column_lines;

    /** Per-column ScRenderLine counts (parallel to `column_lines`). */
    size_t *column_line_counts;
} Row;


// Forward declarations indented to reflect call hierarchy
static size_t build_column_lines(Row *self);
    static void build_continuation_column(Row *self, size_t col);
    static size_t build_normal_column(
        Row *self, size_t col, const ScCell *cell
    );
        static int colspan_total_width(
            const Table *table, size_t col, int span_cols
        );

static void render_visual_line(const Row *self, int visual_line);
    static void render_row_cell(
        const Row *self, size_t col, size_t col_iter,
        int span_cols, int visual_line
    );
        static ScColor resolve_cell_bg(const Row *self, size_t col);
        static void resolve_cell_align(
            const Row *self, size_t col,
            ScHAlign *out_halign, ScVAlign *out_valign
        );
        static int rowspan_cell_content_line(
            const RowSpan *span, int visual_line, int content_lines
        );
        static int normal_cell_content_line(
            const Row *self, int visual_line,
            int content_lines, ScVAlign valign
        );
        static void render_cell_line(
            const Row *self, size_t col, const ScRenderLine *line,
            ScColor cell_bg, int content_width, ScHAlign halign
        );
            static void render_cell_line_aligned(
                const Row *self, size_t col, const ScRenderLine *line,
                ScColor cell_bg, int align_padding, ScHAlign halign
            );
                static ScTextStyle apply_default_span_style(
                    const Row *self, size_t col, ScTextStyle span_style
                );
            static void render_cell_line_truncated(
                const Row *self, size_t col, const ScRenderLine *line,
                ScColor cell_bg, int col_width
            );
        static void render_cell_vertical_separator(
            const Row *self, size_t col, size_t col_iter, int span_cols
        );

static void free_column_lines(Row *self);


/* ── Public entry point ─────────────────────────────────────────────────── */

/**
 * Renders one row (header, data or footer).
 *
 * Builds the per-column TLines, computes the row height, then prints each
 * visual line of the row by delegating to `render_visual_line`.
 *
 * @param table                Outer render context (modified during render).
 * @param cells                Cells of the row; one entry per column.
 * @param row_bg               Background color; `SC_ANSI_COLOR_NONE` = none.
 * @param section              Row section (data / header / footer).
 * @param explicit_row_height  Pre-computed height; `0` = derive from content.
 */
void render_row(
    Table *table, const ScCell *cells, ScColor row_bg,
    RowSection section, int explicit_row_height
) {
    size_t column_count = table->table_data->column_count;
    Row self = {
        .table = table,
        .cells = cells,
        .row_bg = row_bg,
        .section = section,
        // calloc: overflow-checked count*size and zero-initialized slots.
        .column_lines = calloc(column_count, sizeof(ScRenderLine *)),
        .column_line_counts = calloc(column_count, sizeof(size_t)),
    };
    if (!self.column_lines || !self.column_line_counts) {
        free(self.column_lines);
        free(self.column_line_counts);
        return;   // OOM: skip this row
    }

    size_t max_content_lines = build_column_lines(&self);
    self.row_height = explicit_row_height > 0
        ? explicit_row_height
        : (int)max_content_lines + table->pad_vertical;
    if (self.row_height < 1) { self.row_height = 1; }

    for (int line = 0; line < self.row_height; line++) {
        render_visual_line(&self, line);
    }

    free_column_lines(&self);
}


/* ── Building the per-column TLines ─────────────────────────────────────── */

/**
 * Builds the `ScRenderLine` array for every column of the row, handling skip
 * cells, rowspan continuations, and colspan-initiating cells.
 * Returns the tallest non-rowspan-starting line count for height purposes.
 */
static size_t build_column_lines(Row *self) {
    const ScTableData *data = self->table->table_data;
    size_t max_lines = 0;

    for (size_t i = 0; i < data->column_count; i++) {
        size_t col = self->table->opts.right_to_left
            ? (data->column_count - 1 - i) : i;
        const ScCell *cell = &self->cells[col];

        if (cell->col_span < 0) {
            self->column_lines[col] = NULL;
            self->column_line_counts[col] = 0;
            continue;
        }
        if (cell->row_span == -1) {
            build_continuation_column(self, col);
            continue;
        }

        size_t lines = build_normal_column(self, col, cell);
        int row_span = cell->row_span;
        if ((row_span == 0 || row_span == 1) && lines > max_lines) {
            max_lines = lines;
        }
    }
    return max_lines;
}

/**
 * Builds the lines for a column that continues a rowspan from an earlier
 * row. Uses the originating cell stored in `table->row_span`; falls back
 * to an empty entry when no active span context exists.
 */
static void build_continuation_column(Row *self, size_t col) {
    const ScTableData *data = self->table->table_data;
    const RowSpan *span = self->table->row_span
        ? &self->table->row_span[col] : NULL;
    if (!span || !span->cell) {
        self->column_lines[col] = NULL;
        self->column_line_counts[col] = 0;
        return;
    }

    int content_width =
        self->table->column_widths[col]
        - self->table->pad_left - self->table->pad_right;
    if (content_width < 0) { content_width = 0; }

    bool wrap = data->columns[col].opts.word_wrap && content_width > 0;
    self->column_lines[col] = wrap
        ? wrap_cell_lines(
              span->cell, content_width, &self->column_line_counts[col]
          )
        : make_cell_lines(span->cell, &self->column_line_counts[col]);
}

/**
 * Builds the lines for a normal cell or a colspan-initiating cell.
 * Selects word-wrap or plain building based on the column settings.
 * Returns the resulting line count.
 */
static size_t build_normal_column(
    Row *self, size_t col, const ScCell *cell
) {
    const ScTableData *data = self->table->table_data;
    int col_span = cell->col_span;
    int span_cols = (col_span > 1) ? col_span : 1;
    if ((int)col + span_cols > (int)data->column_count) {
        span_cols = (int)data->column_count - (int)col;
    }

    int total_width = colspan_total_width(self->table, col, span_cols);
    int content_width =
        total_width - self->table->pad_left - self->table->pad_right;
    if (content_width < 0) { content_width = 0; }

    bool wrap = data->columns[col].opts.word_wrap && content_width > 0;
    self->column_lines[col] = wrap
        ? wrap_cell_lines(
              cell, content_width, &self->column_line_counts[col]
          )
        : make_cell_lines(cell, &self->column_line_counts[col]);
    return self->column_line_counts[col];
}

/**
 * Returns the total width of a colspan starting at `col` and spanning
 * `span_cols` columns, absorbing inner vertical separators into the sum.
 */
static int colspan_total_width(
    const Table *table, size_t col, int span_cols
) {
    int total = 0;
    for (int i = 0; i < span_cols; i++) {
        size_t current_col = col + (size_t)i;
        total += table->column_widths[current_col];
        bool has_neighbor = (i < span_cols - 1);
        if (has_neighbor
            && table_has_vertical_separator_after(table, current_col)) {
            total++;
        }
    }
    return total;
}


/* ── Rendering one visual line of the row ───────────────────────────────── */

/**
 * Renders one visual line of the row: prints the left frame, iterates
 * columns (handling colspan by skipping continuation iterations), then
 * prints the right frame and newline.
 */
static void render_visual_line(const Row *self, int visual_line) {
    const ScTableData *data = self->table->table_data;
    ScBorderType style = self->table->opts.border.type;
    ScColor outer_color = self->table->opts.border.outer_color;
    bool no_outer = self->table->opts.border.no_outer;
    bool right_to_left = self->table->opts.right_to_left;

    print_left_margin(self->table);
    if (!no_outer) {
        print_colored_string(border_char_sets[style].v, outer_color);
    }

    size_t col_iter = 0;
    while (col_iter < data->column_count) {
        size_t col = right_to_left
            ? (data->column_count - 1 - col_iter) : col_iter;
        int col_span = self->cells[col].col_span;
        if (col_span < 0) { col_iter++; continue; }

        int span_cols = (col_span > 1) ? col_span : 1;
        if ((int)col + span_cols > (int)data->column_count) {
            span_cols = (int)data->column_count - (int)col;
        }

        render_row_cell(self, col, col_iter, span_cols, visual_line);
        col_iter += (size_t)span_cols;
    }

    if (!no_outer) {
        print_colored_string(border_char_sets[style].v, outer_color);
    }
    fputc('\n', sc_output_stream());
}

/**
 * Renders one cell (padding + content + separator) at the given visual
 * line index. Resolves background, alignment, and content-line index,
 * then delegates the actual drawing.
 */
static void render_row_cell(
    const Row *self, size_t col, size_t col_iter,
    int span_cols, int visual_line
) {
    int total_width = colspan_total_width(self->table, col, span_cols);
    int content_width =
        total_width - self->table->pad_left - self->table->pad_right;
    if (content_width < 0) { content_width = 0; }

    ScColor cell_bg = resolve_cell_bg(self, col);
    ScHAlign halign;
    ScVAlign valign;
    resolve_cell_align(self, col, &halign, &valign);

    int content_count = self->column_lines[col]
        ? (int)self->column_line_counts[col] : 0;
    int row_span = self->cells[col].row_span;
    bool is_rowspan_cell = (row_span > 1 || row_span == -1)
        && self->table->row_span
        && self->table->row_span[col].cell;

    int content_line = is_rowspan_cell
        ? rowspan_cell_content_line(
              &self->table->row_span[col], visual_line, content_count
          )
        : normal_cell_content_line(
              self, visual_line, content_count, valign
          );

    print_spaces_with_bg(self->table->pad_left, cell_bg);
    bool has_content = content_line >= 0
        && content_line < content_count
        && self->column_lines[col];
    if (has_content) {
        render_cell_line(
            self, col, &self->column_lines[col][content_line], cell_bg,
            content_width, halign
        );
    } else {
        print_spaces_with_bg(content_width, cell_bg);
    }
    print_spaces_with_bg(self->table->pad_right, cell_bg);

    render_cell_vertical_separator(self, col, col_iter, span_cols);
}

/**
 * Resolves the effective cell background. Priority:
 * `header.row_bg` → `header.col_bg` (header column) → `footer.row_bg` →
 * `footer.col_bg` → explicit row bg → column bg.
 */
static ScColor resolve_cell_bg(const Row *self, size_t col) {
    const ScTableData *data = self->table->table_data;
    bool is_header = (self->section == ROW_SECTION_HEADER);
    bool is_footer = (self->section == ROW_SECTION_FOOTER);
    bool is_header_col = table_is_header_column(self->table, col);

    ScColor col_bg = data->columns[col].opts.bg;
    ScColor cell_bg = sc_color_is_active(self->row_bg) ? self->row_bg : col_bg;

    if (is_header_col && !is_header) {
        cell_bg = is_footer
            ? self->table->opts.footer.col_bg
            : self->table->opts.header.col_bg;
    }
    if (is_header) { cell_bg = self->table->opts.header.row_bg; }
    if (is_footer && !is_header_col) {
        cell_bg = self->table->opts.footer.row_bg;
    }
    if (is_footer && is_header_col) {
        cell_bg = self->table->opts.footer.col_bg;
    }
    return cell_bg;
}

/**
 * Resolves horizontal and vertical alignment for column `col`, applying
 * per-cell overrides on top of the column defaults.
 */
static void resolve_cell_align(
    const Row *self, size_t col,
    ScHAlign *out_halign, ScVAlign *out_valign
) {
    const ScTableData *data = self->table->table_data;
    *out_halign = data->columns[col].opts.halign;
    *out_valign = data->columns[col].opts.valign;
    if (self->cells[col].halign_set)  { *out_halign = self->cells[col].halign; }
    if (self->cells[col].valign_set) { *out_valign = self->cells[col].valign; }
}

/**
 * Returns the content-line index for a rowspan cell at `visual_line`,
 * distributing lines across the span height per the cell's vertical
 * alignment.
 */
static int rowspan_cell_content_line(
    const RowSpan *span, int visual_line, int content_lines
) {
    int extra = span->row_count - content_lines;
    if (extra < 0) { extra = 0; }
    int top_offset = 0;
    if (span->valign == SC_VALIGN_MIDDLE) { top_offset = extra / 2; }
    else if (span->valign == SC_VALIGN_BOTTOM) { top_offset = extra; }
    return span->line_offset + visual_line - top_offset;
}

/**
 * Returns the content-line index for a normal (non-rowspan) cell at
 * `visual_line`, applying vertical alignment within the row height.
 */
static int normal_cell_content_line(
    const Row *self, int visual_line,
    int content_lines, ScVAlign valign
) {
    int extra = self->row_height - content_lines - self->table->pad_vertical;
    if (extra < 0) { extra = 0; }
    int top_pad = self->table->pad_top;
    if (valign == SC_VALIGN_MIDDLE)  { top_pad += extra / 2; }
    else if (valign == SC_VALIGN_BOTTOM) { top_pad += extra; }
    return visual_line - top_pad;
}


/* ── Drawing one ScRenderLine inside a cell ─────────────────────────────── */

/** Dispatches to aligned or truncated rendering based on line vs width. */
static void render_cell_line(
    const Row *self, size_t col, const ScRenderLine *line, ScColor cell_bg,
    int content_width, ScHAlign halign
) {
    int padding = content_width - (int)line->visible_width;
    if (padding >= 0) {
        render_cell_line_aligned(self, col, line, cell_bg, padding, halign);
    } else {
        render_cell_line_truncated(self, col, line, cell_bg, content_width);
    }
}

/**
 * Prints `line` with left/right alignment padding totalling `align_padding`,
 * applying section/column default styling to unstyled spans.
 */
static void render_cell_line_aligned(
    const Row *self, size_t col, const ScRenderLine *line, ScColor cell_bg,
    int align_padding, ScHAlign halign
) {
    int left_pad = 0;
    int right_pad = align_padding;
    if (halign == SC_ALIGN_CENTER) {
        left_pad = align_padding / 2;
        right_pad = align_padding - left_pad;
    } else if (halign == SC_ALIGN_RIGHT) {
        left_pad = align_padding;
        right_pad = 0;
    }

    print_spaces_with_bg(left_pad, cell_bg);
    for (size_t i = 0; i < line->count; i++) {
        ScTextStyle style = apply_default_span_style(
            self, col, line->spans[i].style
        );
        print_span_with_bg(line->spans[i].text, style, cell_bg);
    }
    print_spaces_with_bg(right_pad, cell_bg);
}

/**
 * Returns `span_style` with default styling applied to its unstyled fields
 * (`attr == 0` / `fg.index == 0`). Spans that carry their own styling pass
 * through unchanged. Priority: per-span style > header/footer section style
 * > per-column style (`ScColOpts.style`).
 */
static ScTextStyle apply_default_span_style(
    const Row *self, size_t col, ScTextStyle span_style
) {
    // Section default (header/footer rows).
    ScTextStyle section_style = { 0 };
    if (self->section == ROW_SECTION_HEADER) {
        section_style = self->table->opts.header.style;
    } else if (self->section == ROW_SECTION_FOOTER) {
        section_style = self->table->opts.footer.style;
    }
    if (span_style.attr == 0) { span_style.attr = section_style.attr; }
    if (span_style.fg.index == 0) { span_style.fg = section_style.fg; }

    // Column default fills whatever per-span and section styling left unset.
    ScTextStyle col_style = self->table->table_data->columns[col].opts.style;
    if (span_style.attr == 0) { span_style.attr = col_style.attr; }
    if (span_style.fg.index == 0) { span_style.fg = col_style.fg; }
    return span_style;
}

/**
 * Prints as many columns of `line` as fit within `col_width`, truncating
 * the last span at the exact column boundary when needed.
 */
static void render_cell_line_truncated(
    const Row *self, size_t col, const ScRenderLine *line, ScColor cell_bg,
    int col_width
) {
    int remaining = col_width;
    for (size_t i = 0; i < line->count && remaining > 0; i++) {
        ScTextStyle style = apply_default_span_style(
            self, col, line->spans[i].style
        );
        const char *text = line->spans[i].text;
        int span_width = (int)sc_utf8_string_length(text, strlen(text));

        if (span_width <= remaining) {
            print_span_with_bg(text, style, cell_bg);
            remaining -= span_width;
            continue;
        }
        size_t byte_count = sc_utf8_trim_to_cols(text, remaining);
        char *clipped = strndup(text, byte_count);
        print_span_with_bg(clipped, style, cell_bg);
        free(clipped);
        remaining = 0;
    }
}

/**
 * Prints the vertical separator after the cell at column `col`, choosing
 * the header-column color when the boundary is at the header-column edge.
 */
static void render_cell_vertical_separator(
    const Row *self, size_t col, size_t col_iter, int span_cols
) {
    const ScTableData *data = self->table->table_data;
    bool is_last_in_row =
        col_iter + (size_t)span_cols >= data->column_count;
    if (is_last_in_row) { return; }
    if (self->table->opts.border.type == SC_BORDER_NONE) { return; }

    size_t end_col = col + (size_t)span_cols - 1;
    if (!table_has_vertical_separator_after(self->table, end_col)) {
        return;
    }

    ScColor sep_color = self->table->opts.border.inner_color;
    bool is_header_edge = table_is_header_column(self->table, end_col);
    if (is_header_edge
        && self->table->opts.border.header_col_sep_color.index != 0) {
        sep_color = self->table->opts.border.header_col_sep_color;
    }
    print_colored_string(
        border_char_sets[self->table->opts.border.type].v, sep_color
    );
}


/* ── Cleanup ────────────────────────────────────────────────────────────── */

/** Frees the per-column ScRenderLine arrays built for this row. */
static void free_column_lines(Row *self) {
    size_t column_count = self->table->table_data->column_count;
    for (size_t col = 0; col < column_count; col++) {
        if (self->column_lines[col]) {
            free_tlines(
                self->column_lines[col], self->column_line_counts[col]
            );
        }
    }
    free(self->column_lines);
    free(self->column_line_counts);
}
