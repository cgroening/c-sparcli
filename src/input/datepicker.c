#include "input_internal.h"

#include <stdio.h>
#include <string.h>
#include <time.h>


/** Render-time state for a single date picker. */
typedef struct DateState {
    /** The viewed/selected date. */
    struct tm cur;

    /** WDAYS offset: 0 = Sunday, 1 = Monday. */
    int week_start;
    ScColor accent;
    ScDatePickerOpts opts;
} DateState;

static const char *const MONTHS[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};
static const char *const WDAYS[] = {
    "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"
};
static const char *const DEFAULT_HINT =
    "\xe2\x86\x90/\xe2\x86\x92/\xe2\x86\x91/\xe2\x86\x93 move \xc2\xb7 "
    "pgup/pgdn month \xc2\xb7 shift+pgup/pgdn year \xc2\xb7 "
    "enter select \xc2\xb7 esc cancel";


static DateState make_state(const struct tm *seed, ScDatePickerOpts opts);
static ScRendered *date_render(void *state);
    static void append_weekday_row(ScText *text, int week_start,
                                   ScTextStyle style);
    static void append_day_grid(const DateState *self, ScText *text,
                                ScTextStyle selected_style);
        static int days_in_month(int year, int month0);
            static bool is_leap(int year);
        static int first_weekday(struct tm cur);
static void date_on_key(void *state, ScKey key, bool *done, bool *cancel);
    static void shift_day(DateState *self, int delta);
    static void shift_month(DateState *self, int delta);
    static void shift_year(DateState *self, int delta);


ScInputStatus sc_datepicker(struct tm *io, ScDatePickerOpts opts) {
    if (!io) {
        return SC_INPUT_ERROR;
    }
    sc_theme_apply_datepicker(&opts);

    DateState state = make_state(io, opts);
    ScPromptVTable vtable = {
        .render = date_render,
        .on_key = date_on_key,
    };
    ScPromptShortcuts sk = {
        opts.shortcuts, opts.n_shortcuts, opts.out_shortcut_id
    };
    ScInputStatus status =
        sc_prompt_run(&vtable, &state, opts.shortcuts ? &sk : NULL, NULL);

    if (status == SC_INPUT_OK) {
        state.cur.tm_hour = 0;
        state.cur.tm_min = 0;
        state.cur.tm_sec = 0;
        mktime(&state.cur);
        *io = state.cur;
        if (!opts.hide_summary) {
            char buf[64];
            char line[128];
            strftime(buf, sizeof buf, "%Y-%m-%d", &state.cur);
            snprintf(line, sizeof line, "? %s %s",
                     opts.prompt ? opts.prompt : "Date", buf);
            sc_println(line, opts.summary_style);
        }
    }
    return status;
}

ScRendered *sc_datepicker_frame(const struct tm *seed, ScDatePickerOpts opts) {
    DateState state = make_state(seed, opts);
    return date_render(&state);
}

/** Builds a DateState; a zeroed/NULL `seed` starts at today. */
static DateState make_state(const struct tm *seed, ScDatePickerOpts opts) {
    DateState state = {
        .week_start = (opts.week_start == SC_WEEK_START_SUNDAY) ? 0 : 1,
        .accent = opts.accent.index ? opts.accent : SC_ANSI_COLOR_CYAN,
        .opts = opts,
    };
    bool empty = !seed
        || (seed->tm_year == 0 && seed->tm_mon == 0 && seed->tm_mday == 0);
    if (empty) {
        time_t now = time(NULL);
        struct tm *local_time = localtime(&now);
        if (local_time) {
            state.cur = *local_time;
        }
    } else {
        state.cur = *seed;
    }
    state.cur.tm_hour = 12;   // avoid DST edge effects
    mktime(&state.cur);
    return state;
}

static ScRendered *date_render(void *state) {
    DateState *self = state;

    ScTextStyle prompt_style = sc_style_set(self->opts.prompt_style)
        ? self->opts.prompt_style
        : (ScTextStyle){ SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE,
                         SC_ANSI_COLOR_NONE };
    ScTextStyle header_style = sc_style_set(self->opts.header_style)
        ? self->opts.header_style
        : (ScTextStyle){ SC_TEXT_ATTR_BOLD, self->accent, SC_ANSI_COLOR_NONE };
    ScTextStyle weekday_style = sc_style_set(self->opts.weekday_style)
        ? self->opts.weekday_style
        : (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                         SC_ANSI_COLOR_NONE };
    ScTextStyle selected_style = sc_style_set(self->opts.selected_style)
        ? self->opts.selected_style
        : (ScTextStyle){ SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_BLACK, self->accent };
    const char *prev_glyph = self->opts.header_prev
        ? self->opts.header_prev : "\xe2\x80\xb9";   // ‹
    const char *next_glyph = self->opts.header_next
        ? self->opts.header_next : "\xe2\x80\xba";   // ›

    ScText *text = sc_text_new();
    if (!text) {
        return NULL;
    }

    if (self->opts.prompt || self->opts.prompt_text) {
        sc_prompt_append(text, self->opts.prompt, prompt_style,
                         self->opts.prompt_markup, self->opts.prompt_text);
        sc_text_append(text, "\n", (ScTextStyle){ 0 });
    }

    int year = self->cur.tm_year + 1900;
    char header[96];
    snprintf(header, sizeof header, "  %s %s %d %s", prev_glyph,
             MONTHS[self->cur.tm_mon], year, next_glyph);
    sc_text_append(text, header, header_style);
    sc_text_append(text, "\n", (ScTextStyle){ 0 });

    append_weekday_row(text, self->week_start, weekday_style);
    append_day_grid(self, text, selected_style);

    ScRendered *body = sc_capture_text(text);
    sc_text_free(text);
    return sc_compose_hint(body,
                           self->opts.hint ? self->opts.hint : DEFAULT_HINT,
                           self->opts.hint_layout, self->opts.hint_pos,
                           self->opts.hint_style);
}

/** Appends the weekday header row, rotated to start on `week_start`. */
static void append_weekday_row(ScText *text, int week_start,
                               ScTextStyle style) {
    for (int i = 0; i < 7; i++) {
        sc_text_append(text, WDAYS[(week_start + i) % 7], style);
        if (i < 6) {
            sc_text_append(text, " ", (ScTextStyle){ 0 });
        }
    }
    sc_text_append(text, "\n", (ScTextStyle){ 0 });
}

/** Appends the month's day grid, highlighting the selected day. */
static void append_day_grid(const DateState *self, ScText *text,
                            ScTextStyle selected_style) {
    int year = self->cur.tm_year + 1900;
    int month_days = days_in_month(year, self->cur.tm_mon);
    int selected_day = self->cur.tm_mday;
    int leading = (first_weekday(self->cur) - self->week_start + 7) % 7;

    int col = 0;
    for (int i = 0; i < leading; i++) {
        sc_text_append(text, (col < 6) ? "   " : "  ", (ScTextStyle){ 0 });
        col++;
    }
    for (int day = 1; day <= month_days; day++) {
        char cell[8];
        snprintf(cell, sizeof cell, "%2d", day);
        sc_text_append(
            text, cell,
            day == selected_day ? selected_style : (ScTextStyle){ 0 }
        );
        if (col < 6) {
            sc_text_append(text, " ", (ScTextStyle){ 0 });
        }
        col++;
        if (col == 7 && day < month_days) {
            sc_text_append(text, "\n", (ScTextStyle){ 0 });
            col = 0;
        }
    }
}

/** Number of days in `month0` (0-based) of `year`. */
static int days_in_month(int year, int month0) {
    static const int DAYS[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    };
    if (month0 == 1 && is_leap(year)) {
        return 29;
    }
    return DAYS[month0];
}

static bool is_leap(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/** Weekday (0 = Sunday) of the first day of the viewed month. */
static int first_weekday(struct tm cur) {
    cur.tm_mday = 1;
    cur.tm_hour = 12;   // avoid DST edge effects
    mktime(&cur);
    return cur.tm_wday;
}

static void date_on_key(void *state, ScKey key, bool *done, bool *cancel) {
    (void)cancel;
    DateState *self = state;
    switch (key.type) {
        case SC_KEY_LEFT:           shift_day(self, -1);    break;
        case SC_KEY_RIGHT:          shift_day(self, +1);    break;
        case SC_KEY_UP:             shift_day(self, -7);    break;
        case SC_KEY_DOWN:           shift_day(self, +7);    break;
        case SC_KEY_PAGEUP:         shift_month(self, -1);  break;
        case SC_KEY_PAGEDOWN:       shift_month(self, +1);  break;
        case SC_KEY_SHIFT_PAGEUP:   shift_year(self, -1);   break;
        case SC_KEY_SHIFT_PAGEDOWN: shift_year(self, +1);   break;
        case SC_KEY_ENTER:          *done = true;           return;
        case SC_KEY_CHAR:
            if (key.mods != 0) { return; }   // Ctrl/Alt + char isn't '<'/'>'
            if (key.bytes[0] == '<') {
                shift_month(self, -1);
            } else if (key.bytes[0] == '>') {
                shift_month(self, +1);
            }
            break;
        default:
            return;
    }
}

/** Shifts the selected day by `delta`, normalizing across month/year. */
static void shift_day(DateState *self, int delta) {
    self->cur.tm_mday += delta;
    self->cur.tm_hour = 12;
    mktime(&self->cur);
}

/**
 * Shifts the viewed month by `delta`, keeping the selected day where possible
 * (clamped to the last valid day of the target month, e.g. Jan 31 -> Feb 28).
 */
static void shift_month(DateState *self, int delta) {
    int month = self->cur.tm_mon + delta;
    int year = self->cur.tm_year + 1900;
    while (month < 0) {
        month += 12;
        year--;
    }
    while (month > 11) {
        month -= 12;
        year++;
    }
    int last_day = days_in_month(year, month);
    if (self->cur.tm_mday > last_day) {
        self->cur.tm_mday = last_day;
    }
    self->cur.tm_mon = month;
    self->cur.tm_year = year - 1900;
    self->cur.tm_hour = 12;
    mktime(&self->cur);
}

/**
 * Shifts the viewed year by `delta`, keeping the selected day where possible
 * (clamped, so Feb 29 -> Feb 28 in a non-leap year).
 */
static void shift_year(DateState *self, int delta) {
    int year = self->cur.tm_year + 1900 + delta;
    int last_day = days_in_month(year, self->cur.tm_mon);
    if (self->cur.tm_mday > last_day) {
        self->cur.tm_mday = last_day;
    }
    self->cur.tm_year = year - 1900;
    self->cur.tm_hour = 12;
    mktime(&self->cur);
}
