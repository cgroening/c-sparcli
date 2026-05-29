#include "sparcli.h"
#include "internal.h"

#include <stdio.h>


// Forward declarations indented to reflect call hierarchy
static void print_padded_line(
    const char *line, int left_pad, int right_pad
);

static int get_align_left_pad(ScHAlign align, int spare);


/* ── Padding ─────────────────────────────────────────────────────────────── */

void sc_pad_print(const ScRendered *rendered, ScPadOpts opts) {
    if (!rendered) { return; }

    sc_print_newlines(opts.top);
    for (size_t i = 0; i < rendered->line_count; i++) {
        print_padded_line(rendered->lines[i], opts.left, opts.right);
    }
    sc_print_newlines(opts.bottom);
}

void sc_pad_str(const char *str, ScPadOpts opts) {
    ScRendered *rendered = sc_capture_str(str);
    sc_pad_print(rendered, opts);
    sc_rendered_free(rendered);
}

void sc_pad_text(const ScText *text, ScPadOpts opts) {
    ScRendered *rendered = sc_capture_text(text);
    sc_pad_print(rendered, opts);
    sc_rendered_free(rendered);
}


/* ── Alignment ───────────────────────────────────────────────────────────── */

void sc_align_print(const ScRendered *rendered, ScHAlign align, int width) {
    if (!rendered) { return; }
    if (width <= 0) { width = sc_terminal_width(); }

    for (size_t i = 0; i < rendered->line_count; i++) {
        int spare = width - rendered->column_widths[i];
        if (spare < 0) { spare = 0; }
        int left_pad = get_align_left_pad(align, spare);

        sc_print_spaces(left_pad);
        fputs(rendered->lines[i], sc_output_stream());
        fputc('\n', sc_output_stream());
    }
}

void sc_align_str(const char *str, ScHAlign align, int width) {
    ScRendered *rendered = sc_capture_str(str);
    sc_align_print(rendered, align, width);
    sc_rendered_free(rendered);
}

void sc_align_text(const ScText *text, ScHAlign align, int width) {
    ScRendered *rendered = sc_capture_text(text);
    sc_align_print(rendered, align, width);
    sc_rendered_free(rendered);
}


/** Prints one content line with `left_pad` spaces before and `right_pad` after. */
static void print_padded_line(
    const char *line, int left_pad, int right_pad
) {
    sc_print_spaces(left_pad);
    fputs(line, sc_output_stream());
    sc_print_spaces(right_pad);
    fputc('\n', sc_output_stream());
}



/**
 * Returns the number of left-padding spaces needed to align content of
 * width `(width - spare)` within `width` columns: `spare` for RIGHT,
 * `spare / 2` for CENTER, `0` for LEFT.
 */
static int get_align_left_pad(ScHAlign align, int spare) {
    if (align == SC_ALIGN_RIGHT)  { return spare; }
    if (align == SC_ALIGN_CENTER) { return spare / 2; }
    return 0;
}
