#include "input_internal.h"

#include <stdio.h>
#include <string.h>
#include <time.h>


typedef struct {
    struct tm cur;     /* the viewed/selected date */
    int       week_start;
    ScColor   accent;
    const char *prompt;
} DateState;

static const char *MONTHS[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};
static const char *WDAYS[] = { "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa" };


static bool is_leap(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int days_in_month(int year, int mon0) {
    static const int d[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (mon0 == 1 && is_leap(year)) { return 29; }
    return d[mon0];
}

/** Weekday (0=Sun) of the first day of the viewed month. */
static int first_weekday(struct tm cur) {
    cur.tm_mday = 1;
    cur.tm_hour = 12;  /* avoid DST edge effects */
    mktime(&cur);
    return cur.tm_wday;
}

/** Shifts the selected day by `delta`, normalizing across month/year. */
static void shift_day(DateState *s, int delta) {
    s->cur.tm_mday += delta;
    s->cur.tm_hour  = 12;
    mktime(&s->cur);
}

/** Shifts the viewed month by `delta`, clamping the day to a valid value. */
static void shift_month(DateState *s, int delta) {
    s->cur.tm_mon += delta;
    /* Normalize the month/year, then clamp the day. */
    s->cur.tm_mday = 1;
    s->cur.tm_hour = 12;
    mktime(&s->cur);
    /* tm_mon/tm_year now valid; nothing else needed since day is 1. */
}

static ScRendered *date_render(void *state) {
    DateState *s = state;
    int year = s->cur.tm_year + 1900;
    int mon0 = s->cur.tm_mon;
    int dim  = days_in_month(year, mon0);
    int sel  = s->cur.tm_mday;
    int lead = (first_weekday(s->cur) - s->week_start + 7) % 7;

    ScText *t = sc_text_new();
    if (!t) { return NULL; }

    if (s->prompt) {
        sc_text_append(t, s->prompt, (ScTextStyle){ SC_TEXT_ATTR_BOLD,
                       SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE });
        sc_text_append(t, "\n", (ScTextStyle){ 0 });
    }

    char header[64];
    snprintf(header, sizeof header, "  \xe2\x80\xb9 %s %d \xe2\x80\xba",
             MONTHS[mon0], year);          /* ‹ Month Year › */
    sc_text_append(t, header, (ScTextStyle){ SC_TEXT_ATTR_BOLD, s->accent,
                   SC_ANSI_COLOR_NONE });
    sc_text_append(t, "\n", (ScTextStyle){ 0 });

    /* Weekday header row. */
    ScTextStyle dimst = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
    for (int c = 0; c < 7; c++) {
        sc_text_append(t, WDAYS[(s->week_start + c) % 7], dimst);
        if (c < 6) { sc_text_append(t, " ", (ScTextStyle){ 0 }); }
    }
    sc_text_append(t, "\n", (ScTextStyle){ 0 });

    /* Day grid. */
    int col = 0;
    for (int i = 0; i < lead; i++) {
        sc_text_append(t, (col < 6) ? "   " : "  ", (ScTextStyle){ 0 });
        col++;
    }
    for (int day = 1; day <= dim; day++) {
        char cell[8];
        snprintf(cell, sizeof cell, "%2d", day);
        if (day == sel) {
            ScTextStyle hl = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_BLACK, s->accent };
            sc_text_append(t, cell, hl);
        } else {
            sc_text_append(t, cell, (ScTextStyle){ 0 });
        }
        if (col < 6) { sc_text_append(t, " ", (ScTextStyle){ 0 }); }
        col++;
        if (col == 7 && day < dim) {
            sc_text_append(t, "\n", (ScTextStyle){ 0 });
            col = 0;
        }
    }

    ScRendered *r = sc_capture_text(t);
    sc_text_free(t);
    return r;
}

static void date_on_key(void *state, ScKey key, bool *done, bool *cancel) {
    (void)cancel;
    DateState *s = state;
    switch (key.type) {
        case SC_KEY_LEFT:     shift_day(s, -1); break;
        case SC_KEY_RIGHT:    shift_day(s, +1); break;
        case SC_KEY_UP:       shift_day(s, -7); break;
        case SC_KEY_DOWN:     shift_day(s, +7); break;
        case SC_KEY_PAGEUP:   shift_month(s, -1); break;
        case SC_KEY_PAGEDOWN: shift_month(s, +1); break;
        case SC_KEY_ENTER:    *done = true; return;
        case SC_KEY_CHAR:
            if (key.bytes[0] == '<') { shift_month(s, -1); }
            else if (key.bytes[0] == '>') { shift_month(s, +1); }
            break;
        default:
            return;
    }
}

ScInputStatus sc_datepicker(struct tm *io, ScDatePickerOpts opts) {
    if (!io) { return SC_INPUT_ERROR; }

    DateState s = {
        .week_start = (opts.week_start == 0) ? 1 : opts.week_start,  /* default Mon */
        .accent     = opts.accent.index ? opts.accent : SC_ANSI_COLOR_CYAN,
        .prompt     = opts.prompt,
    };

    /* A zeroed struct tm seeds today; otherwise honor the caller's date. */
    if (io->tm_year == 0 && io->tm_mon == 0 && io->tm_mday == 0) {
        time_t now = time(NULL);
        struct tm *lt = localtime(&now);
        if (lt) { s.cur = *lt; }
    } else {
        s.cur = *io;
    }
    s.cur.tm_hour = 12;
    mktime(&s.cur);

    ScPromptVTable vt = { .render = date_render, .on_key = date_on_key };
    ScInputStatus status = sc_prompt_run(&vt, &s);

    if (status == SC_INPUT_OK) {
        s.cur.tm_hour = 0; s.cur.tm_min = 0; s.cur.tm_sec = 0;
        mktime(&s.cur);
        *io = s.cur;
        char buf[64];
        strftime(buf, sizeof buf, "%Y-%m-%d", &s.cur);
        printf("? %s %s\n", opts.prompt ? opts.prompt : "Date", buf);
    }
    return status;
}
