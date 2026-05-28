#include "sparcli.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** Default mid-color ratio threshold. */
#define DEFAULT_THRESHOLD_MID 0.5

/** Default high-color ratio threshold. */
#define DEFAULT_THRESHOLD_HIGH 0.75

/** Buffer size for the formatted percent string. */
#define PERCENT_BUFFER 16

/** Buffer size for the formatted value/max string. */
#define VALUE_BUFFER 64

/** Buffer size used when emitting a truncated label through `sc_print`. */
#define LABEL_PRINT_BUFFER 512

/** Spacing inserted between the label field and the bar. */
#define LABEL_TRAILING_PAD 2


/** Box-drawing characters for one fill style. */
typedef struct StyleChars {
    /** Cell character used for the body of the filled section. */
    const char *fill;

    /** Edge cell drawn between filled and empty sections; `NULL` = use `fill`. */
    const char *head;

    /** Cell character used for the empty section. */
    const char *empty;
} StyleChars;

static const StyleChars style_chars[] = {
    [SC_PROGRESS_BLOCK]  = { "\xe2\x96\x88", NULL,           "\xe2\x96\x91" },
    [SC_PROGRESS_ASCII]  = { "=",            ">",            " "            },
    [SC_PROGRESS_LINE]   = { "\xe2\x94\x81", NULL,           "\xe2\x95\x8c" },
    [SC_PROGRESS_SHADED] = { "\xe2\x96\x93", "\xe2\x96\x92", "\xe2\x96\x91" },
};


/** Persistent progress bar state held between frames. */
struct ScProgressBar {
    /** Rendering options captured at construction. */
    ScProgressBarOpts opts;

    /** Heap-allocated label string; `NULL` when no label is set. */
    char *label;
};

/** Per-frame computed layout values for one render call. */
typedef struct Frame {
    /** Bar instance being rendered; not owned. */
    ScProgressBar *bar;

    /** Clamped progress ratio in `[0.0, 1.0]`. */
    double ratio;

    /** When `true`, `max > 0` so value/max display is meaningful. */
    bool has_max;

    /** Visible width of the left cap; `0` when no cap. */
    int left_cap_width;

    /** Visible width of the right cap; `0` when no cap. */
    int right_cap_width;

    /** Label column width including the trailing pad; `0` when no label. */
    int label_field_width;

    /** Visible width of `percent_text`; `0` when `show_percent` is off. */
    int percent_width;

    /**
     * Width reserved for the value field so the bar does not jitter while
     * the value digit count changes; `0` when `show_value` is off.
     */
    int value_reserve_width;

    /** Number of cells available for the bar body. */
    int bar_width;

    /** Number of filled cells (`<= bar_width`). */
    int filled_cells;

    /** Fill color resolved from `opts.fill_color` and thresholds. */
    ScColor fill_color;

    /** Pre-formatted ` XX%` string; empty when `show_percent` is off. */
    char percent_text[PERCENT_BUFFER];

    /** Pre-formatted ` (value/max)` string; empty when `show_value` is off. */
    char value_text[VALUE_BUFFER];
} Frame;


// Forward declarations indented to reflect call hierarchy
static void render(ScProgressBar *bar, double value, double max, bool final);
    static void init_frame(
        Frame *frame, ScProgressBar *bar, double value, double max
    );
        static double clamp_ratio(double value, double max);
        static int cap_width(const char *cap);
        static int label_field_width(const ScProgressBar *bar);
        static void format_percent(Frame *frame, double ratio);
        static void format_value(Frame *frame, double value, double max);
        static int compute_bar_width(const Frame *frame);
        static ScColor resolve_fill_color(
            const ScProgressBarOpts *opts, double ratio
        );
    static void render_frame(const Frame *frame, bool final);
        static void render_label(const Frame *frame);
            static void print_label_text(
                const ScProgressBar *bar, int byte_count
            );
        static void render_cap(const char *cap);
        static void render_bar_body(const Frame *frame);
            static void print_repeat(const char *str, int count, ScColor color);
        static void render_percent(const Frame *frame);
        static void render_value(const Frame *frame);
        static void terminate_line(bool final);


ScProgressBar *sc_progressbar_new(ScProgressBarOpts opts) {
    ScProgressBar *bar = calloc(1, sizeof(ScProgressBar));
    bar->opts = opts;
    return bar;
}

void sc_progressbar_set_label(ScProgressBar *bar, const char *label) {
    if (!bar) { return; }
    free(bar->label);
    bar->label = label ? strdup(label) : NULL;
}

void sc_progressbar_draw(ScProgressBar *bar, double value, double max) {
    if (!bar) { return; }
    render(bar, value, max, false);
}

void sc_progressbar_finish(ScProgressBar *bar, double value, double max) {
    if (!bar) { return; }
    render(bar, value, max, true);
}

void sc_progressbar_free(ScProgressBar *bar) {
    if (!bar) { return; }
    free(bar->label);
    free(bar);
}


/** Builds a frame and renders it; `final` terminates with `\n`, else `\r`. */
static void render(
    ScProgressBar *bar, double value, double max, bool final
) {
    Frame frame;
    init_frame(&frame, bar, value, max);
    render_frame(&frame, final);
}

/** Computes all layout values for a single bar frame. */
static void init_frame(
    Frame *frame, ScProgressBar *bar, double value, double max
) {
    frame->bar = bar;
    frame->ratio = clamp_ratio(value, max);
    frame->has_max = (max > 0.0);

    frame->left_cap_width = cap_width(bar->opts.left_cap);
    frame->right_cap_width = cap_width(bar->opts.right_cap);
    frame->label_field_width = label_field_width(bar);

    format_percent(frame, frame->ratio);
    format_value(frame, value, max);

    frame->bar_width = compute_bar_width(frame);
    frame->fill_color = resolve_fill_color(&bar->opts, frame->ratio);

    int filled = (int)(frame->ratio * frame->bar_width);
    if (filled > frame->bar_width) { filled = frame->bar_width; }
    frame->filled_cells = filled;
}

/**
 * Clamps `value`/`max` into a `[0.0, 1.0]` ratio. When `max <= 0`, `value`
 * is interpreted as the ratio directly.
 */
static double clamp_ratio(double value, double max) {
    double ratio = (max > 0.0) ? value / max : value;
    if (ratio < 0.0) { return 0.0; }
    if (ratio > 1.0) { return 1.0; }
    return ratio;
}

/** Returns the visible column width of `cap`, or `0` when `cap` is `NULL`. */
static int cap_width(const char *cap) {
    return cap ? (int)sc_utf8_string_length(cap, strlen(cap)) : 0;
}

/**
 * Returns the label column width including the trailing pad; `0` when no
 * label is set on `bar`.
 */
static int label_field_width(const ScProgressBar *bar) {
    if (!bar->label) { return 0; }
    int natural_width = (int)sc_utf8_string_length(
        bar->label, strlen(bar->label)
    );
    int configured_width = bar->opts.label_width > 0
        ? bar->opts.label_width : natural_width;
    return configured_width + LABEL_TRAILING_PAD;
}

/** Writes the ` XX%` string into `frame->percent_text`. */
static void format_percent(Frame *frame, double ratio) {
    if (!frame->bar->opts.show_percent) {
        frame->percent_text[0] = '\0';
        frame->percent_width = 0;
        return;
    }
    snprintf(
        frame->percent_text, PERCENT_BUFFER, " %3.0f%%", ratio * 100.0
    );
    frame->percent_width = (int)sc_utf8_string_length(
        frame->percent_text, strlen(frame->percent_text)
    );
}

/**
 * Writes the ` (value/max)` string into `frame->value_text` and reserves
 * the worst-case width so the bar remains stable while the value grows.
 */
static void format_value(Frame *frame, double value, double max) {
    frame->value_text[0] = '\0';
    frame->value_reserve_width = 0;
    if (!frame->bar->opts.show_value || max <= 0.0) { return; }

    char max_text[VALUE_BUFFER];
    long value_int = (long)value;
    long max_int = (long)max;
    bool both_integral = ((double)value_int == value)
        && ((double)max_int == max);

    if (both_integral) {
        snprintf(
            frame->value_text, VALUE_BUFFER,
            " (%ld/%ld)", value_int, max_int
        );
        snprintf(
            max_text, VALUE_BUFFER, " (%ld/%ld)", max_int, max_int
        );
    } else {
        snprintf(
            frame->value_text, VALUE_BUFFER, " (%.1f/%.1f)", value, max
        );
        snprintf(
            max_text, VALUE_BUFFER, " (%.1f/%.1f)", max, max
        );
    }
    frame->value_reserve_width = (int)sc_utf8_string_length(
        max_text, strlen(max_text)
    );
}

/**
 * Returns the cell count for the bar body: `opts.bar_width` if set,
 * otherwise the leftover after subtracting all other fields from `width`
 * (or the terminal width). Clamped to `>= 1`.
 */
static int compute_bar_width(const Frame *frame) {
    if (frame->bar->opts.bar_width > 0) {
        return frame->bar->opts.bar_width;
    }
    int total_width = frame->bar->opts.width > 0
        ? frame->bar->opts.width : sc_terminal_width();
    int available = total_width - frame->label_field_width
                    - frame->left_cap_width - frame->right_cap_width
                    - frame->percent_width - frame->value_reserve_width;
    return available > 0 ? available : 1;
}

/**
 * Returns the fill color for `ratio`: `opts.fill_color` when thresholds are
 * disabled, otherwise the low/mid/high color from `opts.thresholds`.
 */
static ScColor resolve_fill_color(
    const ScProgressBarOpts *opts, double ratio
) {
    if (!opts->thresholds.enabled) { return opts->fill_color; }

    double mid = opts->thresholds.mid > 0.0
        ? opts->thresholds.mid : DEFAULT_THRESHOLD_MID;
    double high = opts->thresholds.high > 0.0
        ? opts->thresholds.high : DEFAULT_THRESHOLD_HIGH;

    ScColor color = opts->thresholds.color_low;
    if (ratio >= mid)  { color = opts->thresholds.color_mid; }
    if (ratio >= high) { color = opts->thresholds.color_high; }
    return color;
}


/** Renders the frame to stdout; `final` terminates with `\n`, else `\r`. */
static void render_frame(const Frame *frame, bool final) {
    render_label(frame);
    render_cap(frame->bar->opts.left_cap);
    render_bar_body(frame);
    render_cap(frame->bar->opts.right_cap);
    render_percent(frame);
    render_value(frame);
    terminate_line(final);
}

/**
 * Prints the label (truncated or padded to the configured field width)
 * followed by the trailing pad. No output when no label is set.
 */
static void render_label(const Frame *frame) {
    const ScProgressBar *bar = frame->bar;
    if (!bar->label) { return; }

    int natural_width = (int)sc_utf8_string_length(
        bar->label, strlen(bar->label)
    );
    int field_width = bar->opts.label_width > 0
        ? bar->opts.label_width : natural_width;
    int print_byte_count = (natural_width > field_width)
        ? (int)sc_utf8_trim_to_cols(bar->label, field_width)
        : (int)strlen(bar->label);
    int printed_width = natural_width < field_width
        ? natural_width : field_width;

    print_label_text(bar, print_byte_count);
    sc_print_spaces(field_width - printed_width);
    sc_print_spaces(LABEL_TRAILING_PAD);
}

/**
 * Emits the first `byte_count` bytes of `bar->label`, applying
 * `opts.label_style` when it carries formatting.
 */
static void print_label_text(const ScProgressBar *bar, int byte_count) {
    ScTextStyle style = bar->opts.label_style;
    bool has_format = style.attr != 0
        || sc_color_is_active(style.fg) || sc_color_is_active(style.bg);

    if (!has_format) {
        fwrite(bar->label, 1, (size_t)byte_count, sc_output_stream());
        return;
    }
    if (byte_count >= LABEL_PRINT_BUFFER) { return; }

    char buffer[LABEL_PRINT_BUFFER];
    memcpy(buffer, bar->label, (size_t)byte_count);
    buffer[byte_count] = '\0';
    sc_print(buffer, style);
}


/** Prints `cap` when non-NULL; otherwise no output. */
static void render_cap(const char *cap) {
    if (cap) { fputs(cap, sc_output_stream()); }
}

/**
 * Prints the bar body: filled cells (with optional edge head) followed by
 * the empty cells.
 */
static void render_bar_body(const Frame *frame) {
    const StyleChars *chars = &style_chars[frame->bar->opts.type];
    const char *fill_char = chars->fill;
    const char *empty_char = chars->empty;
    const char *head_char = chars->head ? chars->head : chars->fill;

    int empty_count = frame->bar_width - frame->filled_cells;
    bool styled_has_edge = (frame->bar->opts.type == SC_PROGRESS_ASCII
        || frame->bar->opts.type == SC_PROGRESS_SHADED);
    bool draw_head = styled_has_edge
        && frame->filled_cells > 0
        && frame->filled_cells < frame->bar_width;

    int solid_count = frame->filled_cells - (draw_head ? 1 : 0);
    print_repeat(fill_char, solid_count, frame->fill_color);
    if (draw_head) {
        print_repeat(head_char, 1, frame->fill_color);
    }
    print_repeat(empty_char, empty_count, frame->bar->opts.empty_color);
}

/**
 * Prints `str` exactly `count` times, wrapped in ANSI color escapes when
 * `color` is set.
 */
static void print_repeat(const char *str, int count, ScColor color) {
    if (count <= 0) { return; }
    bool styled = sc_color_is_active(color);
    if (styled) { sc_apply_colors(color, SC_ANSI_COLOR_NONE); }
    for (int i = 0; i < count; i++) { fputs(str, sc_output_stream()); }
    if (styled) { fputs(SC_ANSI_ESCAPE_CODE_RESET, sc_output_stream()); }
}

/**
 * Returns `true` when `color` should emit ANSI escapes; `SC_ANSI_COLOR_NONE`
 * and zero-initialized `ScColor` both return `false`.
 */

/** Prints `frame->percent_text` when `show_percent` is enabled. */
static void render_percent(const Frame *frame) {
    if (!frame->bar->opts.show_percent) { return; }
    fputs(frame->percent_text, sc_output_stream());
}

/**
 * Prints `frame->value_text` followed by spaces up to the reserved width so
 * the trailing display does not shrink as the value grows.
 */
static void render_value(const Frame *frame) {
    if (!frame->bar->opts.show_value || !frame->has_max) { return; }
    fputs(frame->value_text, sc_output_stream());
    int actual_width = (int)sc_utf8_string_length(
        frame->value_text, strlen(frame->value_text)
    );
    sc_print_spaces(frame->value_reserve_width - actual_width);
}

/** Emits the final line break (`\n` or `\r` + flush). */
static void terminate_line(bool final) {
    if (final) {
        fputc('\n', sc_output_stream());
        return;
    }
    fputc('\r', sc_output_stream());
    fflush(sc_output_stream());
}
