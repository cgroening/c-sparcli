#include "sparcli.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** Maximum number of frames any spinner style uses. */
#define MAX_SPINNER_FRAMES 10

/** Success symbol (`✔`, U+2714). */
#define SUCCESS_SYMBOL "\xe2\x9c\x94"

/** Failure symbol (`✖`, U+2716). */
#define FAILURE_SYMBOL "\xe2\x9c\x96"

/** Visible width of one spinner character. */
#define SPINNER_CHAR_WIDTH 1


static const char *const spinner_frames[4][MAX_SPINNER_FRAMES] = {
    [SC_SPINNER_BRAILLE] = {
        "\xe2\xa0\x8b", "\xe2\xa0\x99", "\xe2\xa0\xb9", "\xe2\xa0\xb8",
        "\xe2\xa0\xbc", "\xe2\xa0\xb4", "\xe2\xa0\xa6", "\xe2\xa0\xa7",
        "\xe2\xa0\x87", "\xe2\xa0\x8f",
    },
    [SC_SPINNER_PIPE] = {
        "|", "/", "-", "\\",
        NULL, NULL, NULL, NULL, NULL, NULL,
    },
    [SC_SPINNER_DOTS] = {
        "\xe2\xa3\xbe", "\xe2\xa3\xbd", "\xe2\xa3\xbb", "\xe2\xa2\xbf",
        "\xe2\xa1\xbf", "\xe2\xa3\x9f", "\xe2\xa3\xaf", "\xe2\xa3\xb7",
        NULL, NULL,
    },
    [SC_SPINNER_ARROW] = {
        "\xe2\x86\x90", "\xe2\x86\x96", "\xe2\x86\x91", "\xe2\x86\x97",
        "\xe2\x86\x92", "\xe2\x86\x98", "\xe2\x86\x93", "\xe2\x86\x99",
        NULL, NULL,
    },
};

static const int spinner_frame_count[] = {
    [SC_SPINNER_BRAILLE] = 10,
    [SC_SPINNER_PIPE]    = 4,
    [SC_SPINNER_DOTS]    = 8,
    [SC_SPINNER_ARROW]   = 8,
};


/** Spinner instance state. */
struct ScSpinner {
    /** Rendering options captured at construction. */
    ScSpinnerOpts opts;

    /** Heap-allocated label; `NULL` when no label is set. */
    char *label;

    /** Index of the next frame to print (0-based). */
    int frame_index;
};


// Forward declarations indented to reflect call hierarchy
static void render_spinner_char(const ScSpinner *spinner);
    static void print_colored(const char *str, ScColor color);
static int render_label(const ScSpinner *spinner);
static void pad_to_terminal_width(int already_printed);

static void clear_current_line(void);
static void render_status_symbol(bool success);


ScSpinner *sc_spinner_new(const char *label, ScSpinnerOpts opts) {
    ScSpinner *spinner = calloc(1, sizeof(ScSpinner));
    if (!spinner) {
        return NULL;
    }
    spinner->opts = opts;
    spinner->label = label ? strdup(label) : NULL;
    return spinner;
}

void sc_spinner_set_label(ScSpinner *spinner, const char *label) {
    if (!spinner) { return; }
    free(spinner->label);
    spinner->label = label ? strdup(label) : NULL;
}

void sc_spinner_tick(ScSpinner *spinner) {
    if (!spinner) { return; }
    render_spinner_char(spinner);
    int visible_width = SPINNER_CHAR_WIDTH + render_label(spinner);
    pad_to_terminal_width(visible_width);

    fputc('\r', sc_output_stream());
    fflush(sc_output_stream());

    int frame_count = spinner_frame_count[spinner->opts.type];
    spinner->frame_index = (spinner->frame_index + 1) % frame_count;
}

void sc_spinner_finish(
    ScSpinner *spinner, bool success, const char *label
) {
    if (!spinner) { return; }
    (void)spinner;

    clear_current_line();
    render_status_symbol(success);

    if (label) {
        fputc(' ', sc_output_stream());
        fputs(label, sc_output_stream());
    }
    fputc('\n', sc_output_stream());
}

void sc_spinner_free(ScSpinner *spinner) {
    if (!spinner) { return; }
    free(spinner->label);
    free(spinner);
}


/** Prints the current animation frame in the configured spinner color. */
static void render_spinner_char(const ScSpinner *spinner) {
    const char *frame =
        spinner_frames[spinner->opts.type][spinner->frame_index];
    print_colored(frame, spinner->opts.color);
}

/**
 * Prints `str` wrapped in ANSI color escapes when `color` is set; prints
 * it as-is otherwise.
 */
static void print_colored(const char *str, ScColor color) {
    if (!sc_color_is_active(color)) {
        fputs(str, sc_output_stream());
        return;
    }
    sc_apply_colors(color, SC_ANSI_COLOR_NONE);
    fputs(str, sc_output_stream());
    fputs(SC_ANSI_ESCAPE_CODE_RESET, sc_output_stream());
}

/**
 * Returns `true` when `color` should emit ANSI escapes; both
 * `SC_ANSI_COLOR_NONE` and a zero-initialized `ScColor` return `false`.
 */

/**
 * Prints `" label"` if a label is set, applying `label_style` when it
 * carries any formatting. Returns the visible width that was printed
 * (`0` when no label).
 */
static int render_label(const ScSpinner *spinner) {
    if (!spinner->label) { return 0; }

    fputc(' ', sc_output_stream());

    ScTextStyle style = spinner->opts.label_style;
    bool style_has_format = style.attr != 0
        || sc_color_is_active(style.fg) || sc_color_is_active(style.bg);
    if (style_has_format) {
        sc_print(spinner->label, style);
    } else {
        fputs(spinner->label, sc_output_stream());
    }

    int label_width = (int)sc_utf8_string_length(
        spinner->label, strlen(spinner->label)
    );
    return 1 + label_width;
}

/**
 * Pads the current line up to the terminal width so a shorter new label
 * cleanly overwrites a longer previous one.
 */
static void pad_to_terminal_width(int already_printed) {
    int terminal_width = sc_terminal_width();
    sc_print_spaces(terminal_width - already_printed);
}


/** Overwrites the current terminal line with spaces, then returns to col 0. */
static void clear_current_line(void) {
    int terminal_width = sc_terminal_width();
    sc_print_spaces(terminal_width);
    fputc('\r', sc_output_stream());
}

/** Prints `✔` in green (success) or `✖` in red (failure). */
static void render_status_symbol(bool success) {
    ScColor color = success ? SC_ANSI_COLOR_GREEN : SC_ANSI_COLOR_RED;
    const char *symbol = success ? SUCCESS_SYMBOL : FAILURE_SYMBOL;
    print_colored(symbol, color);
}
