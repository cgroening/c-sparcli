/**
 * @file cli_parse.c
 * Lookup-table based string parsers for the CLI (enums, colors, edges).
 */
#include "cli_parse.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/** One name-to-value mapping inside a lookup table. */
typedef struct ScCliName {
    const char *name;
    int         value;
} ScCliName;

static bool lookup_value(const ScCliName *table, size_t count,
                         const char *name, int *out);
static bool parse_hex_color(const char *spec, ScColor *out);
static bool parse_rgb_color(const char *spec, ScColor *out);
static bool parse_int_list(const char *spec, int *values, size_t *count);

static const ScCliName BORDER_NAMES[] = {
    { .name = "none",    .value = SC_BORDER_NONE },
    { .name = "ascii",   .value = SC_BORDER_ASCII },
    { .name = "single",  .value = SC_BORDER_SINGLE },
    { .name = "double",  .value = SC_BORDER_DOUBLE },
    { .name = "rounded", .value = SC_BORDER_ROUNDED },
    { .name = "thick",   .value = SC_BORDER_THICK },
};

static const ScCliName HALIGN_NAMES[] = {
    { .name = "left",   .value = SC_ALIGN_LEFT },
    { .name = "center", .value = SC_ALIGN_CENTER },
    { .name = "right",  .value = SC_ALIGN_RIGHT },
};

static const ScCliName ALERT_NAMES[] = {
    { .name = "info",    .value = SC_ALERT_INFO },
    { .name = "debug",   .value = SC_ALERT_DEBUG },
    { .name = "warning", .value = SC_ALERT_WARNING },
    { .name = "error",   .value = SC_ALERT_ERROR },
    { .name = "success", .value = SC_ALERT_SUCCESS },
};

static const ScCliName LIST_MARKER_NAMES[] = {
    { .name = "bullet",      .value = SC_LIST_BULLET },
    { .name = "number",      .value = SC_LIST_NUMBER },
    { .name = "alpha",       .value = SC_LIST_ALPHA_LC },
    { .name = "alpha-upper", .value = SC_LIST_ALPHA_UC },
    { .name = "roman",       .value = SC_LIST_ROMAN_LC },
    { .name = "roman-upper", .value = SC_LIST_ROMAN_UC },
};

static const ScCliName SPINNER_NAMES[] = {
    { .name = "braille", .value = SC_SPINNER_BRAILLE },
    { .name = "pipe",    .value = SC_SPINNER_PIPE },
    { .name = "dots",    .value = SC_SPINNER_DOTS },
    { .name = "arrow",   .value = SC_SPINNER_ARROW },
};

static const ScCliName PROGRESS_NAMES[] = {
    { .name = "block",  .value = SC_PROGRESS_BLOCK },
    { .name = "ascii",  .value = SC_PROGRESS_ASCII },
    { .name = "line",   .value = SC_PROGRESS_LINE },
    { .name = "shaded", .value = SC_PROGRESS_SHADED },
};

static const ScCliName WEEK_START_NAMES[] = {
    { .name = "monday", .value = SC_WEEK_START_MONDAY },
    { .name = "sunday", .value = SC_WEEK_START_SUNDAY },
};

static const ScCliName HINT_LAYOUT_NAMES[] = {
    { .name = "inline",  .value = SC_HINT_INLINE },
    { .name = "stacked", .value = SC_HINT_STACKED },
    { .name = "hidden",  .value = SC_HINT_HIDDEN },
};

static const ScCliName HINT_POS_NAMES[] = {
    { .name = "top",    .value = SC_HINT_POS_TOP },
    { .name = "bottom", .value = SC_HINT_POS_BOTTOM },
    { .name = "left",   .value = SC_HINT_POS_LEFT },
    { .name = "right",  .value = SC_HINT_POS_RIGHT },
};

static const ScCliName STYLE_ATTR_NAMES[] = {
    { .name = "none",      .value = SC_TEXT_ATTR_NONE },
    { .name = "bold",      .value = SC_TEXT_ATTR_BOLD },
    { .name = "dim",       .value = SC_TEXT_ATTR_DIM },
    { .name = "italic",    .value = SC_TEXT_ATTR_ITALIC },
    { .name = "underline", .value = SC_TEXT_ATTR_UNDER },
};

/** Name-to-function mapping for input character filters. */
typedef struct ScCliFilterName {
    const char  *name;
    ScCharFilter filter;
} ScCliFilterName;

static const ScCliFilterName FILTER_NAMES[] = {
    { .name = "digits",   .filter = sc_filter_digits },
    { .name = "decimal",  .filter = sc_filter_decimal },
    { .name = "alpha",    .filter = sc_filter_alpha },
    { .name = "alnum",    .filter = sc_filter_alnum },
    { .name = "no-space", .filter = sc_filter_no_space },
};

#define SC_CLI_TABLE_SIZE(table) (sizeof(table) / sizeof((table)[0]))

bool sc_cli_parse_border(const char *name, ScBorderType *out) {
    int value = 0;
    if (!lookup_value(BORDER_NAMES, SC_CLI_TABLE_SIZE(BORDER_NAMES),
                      name, &value)) {
        return false;
    }
    *out = (ScBorderType)value;
    return true;
}

bool sc_cli_parse_halign(const char *name, ScHAlign *out) {
    int value = 0;
    if (!lookup_value(HALIGN_NAMES, SC_CLI_TABLE_SIZE(HALIGN_NAMES),
                      name, &value)) {
        return false;
    }
    *out = (ScHAlign)value;
    return true;
}

bool sc_cli_parse_alert(const char *name, ScAlertType *out) {
    int value = 0;
    if (!lookup_value(ALERT_NAMES, SC_CLI_TABLE_SIZE(ALERT_NAMES),
                      name, &value)) {
        return false;
    }
    *out = (ScAlertType)value;
    return true;
}

bool sc_cli_parse_list_marker(const char *name, ScListMarker *out) {
    int value = 0;
    if (!lookup_value(LIST_MARKER_NAMES, SC_CLI_TABLE_SIZE(LIST_MARKER_NAMES),
                      name, &value)) {
        return false;
    }
    *out = (ScListMarker)value;
    return true;
}

bool sc_cli_parse_spinner(const char *name, ScSpinnerType *out) {
    int value = 0;
    if (!lookup_value(SPINNER_NAMES, SC_CLI_TABLE_SIZE(SPINNER_NAMES),
                      name, &value)) {
        return false;
    }
    *out = (ScSpinnerType)value;
    return true;
}

bool sc_cli_parse_progress(const char *name, ScProgressType *out) {
    int value = 0;
    if (!lookup_value(PROGRESS_NAMES, SC_CLI_TABLE_SIZE(PROGRESS_NAMES),
                      name, &value)) {
        return false;
    }
    *out = (ScProgressType)value;
    return true;
}

bool sc_cli_parse_week_start(const char *name, ScWeekStart *out) {
    int value = 0;
    if (!lookup_value(WEEK_START_NAMES, SC_CLI_TABLE_SIZE(WEEK_START_NAMES),
                      name, &value)) {
        return false;
    }
    *out = (ScWeekStart)value;
    return true;
}

bool sc_cli_parse_hint_layout(const char *name, ScHintLayout *out) {
    int value = 0;
    if (!lookup_value(HINT_LAYOUT_NAMES, SC_CLI_TABLE_SIZE(HINT_LAYOUT_NAMES),
                      name, &value)) {
        return false;
    }
    *out = (ScHintLayout)value;
    return true;
}

bool sc_cli_parse_hint_pos(const char *name, ScHintPosition *out) {
    int value = 0;
    if (!lookup_value(HINT_POS_NAMES, SC_CLI_TABLE_SIZE(HINT_POS_NAMES),
                      name, &value)) {
        return false;
    }
    *out = (ScHintPosition)value;
    return true;
}

bool sc_cli_parse_filter(const char *name, ScCharFilter *out) {
    for (size_t i = 0; i < SC_CLI_TABLE_SIZE(FILTER_NAMES); i++) {
        if (strcmp(FILTER_NAMES[i].name, name) == 0) {
            *out = FILTER_NAMES[i].filter;
            return true;
        }
    }
    return false;
}

bool sc_cli_parse_color(const char *spec, ScColor *out) {
    /* ANSI names + named RGB palette (orange, accent, error, …). */
    if (sc_color_by_name(spec, out)) {
        return true;
    }
    if (spec[0] == '#') {
        return parse_hex_color(spec, out);
    }
    return parse_rgb_color(spec, out);
}

bool sc_cli_parse_style(const char *spec, ScTextStyle *out) {
    ScTextStyle style = { 0 };
    bool        have_fg = false;

    // Tokenize on whitespace into a private copy (strtok mutates).
    char buffer[256];
    size_t len = strlen(spec);
    if (len >= sizeof buffer) {
        return false;
    }
    memcpy(buffer, spec, len + 1);

    char *save  = NULL;
    char *token = strtok_r(buffer, " \t", &save);
    while (token != NULL) {
        int attr = 0;
        if (lookup_value(STYLE_ATTR_NAMES, SC_CLI_TABLE_SIZE(STYLE_ATTR_NAMES),
                         token, &attr)) {
            style.attr = (attr == SC_TEXT_ATTR_NONE)
                             ? SC_TEXT_ATTR_NONE
                             : (ScTextAttribute)(style.attr
                                                 | (ScTextAttribute)attr);
        } else if (strcmp(token, "on") == 0) {
            token = strtok_r(NULL, " \t", &save);
            if (token == NULL || !sc_cli_parse_color(token, &style.bg)) {
                return false;
            }
        } else if (!have_fg && sc_cli_parse_color(token, &style.fg)) {
            have_fg = true;
        } else {
            return false;   // unknown attribute / second fg / bad color
        }
        token = strtok_r(NULL, " \t", &save);
    }

    *out = style;
    return true;
}

bool sc_cli_parse_edges(const char *spec, ScEdges *out) {
    enum { MAX_EDGE_VALUES = 4 };
    int    values[MAX_EDGE_VALUES] = { 0 };
    size_t count                   = MAX_EDGE_VALUES;
    if (!parse_int_list(spec, values, &count)) {
        return false;
    }

    /* 1 value = all sides; 2 = vertical,horizontal; 4 = top,right,bottom,
       left (CSS order). */
    if (count == 1) {
        *out = (ScEdges){ .top = values[0], .right = values[0],
                          .bottom = values[0], .left = values[0] };
        return true;
    }
    if (count == 2) {
        *out = (ScEdges){ .top = values[0], .right = values[1],
                          .bottom = values[0], .left = values[1] };
        return true;
    }
    if (count == 4) {
        *out = (ScEdges){ .top = values[0], .right = values[1],
                          .bottom = values[2], .left = values[3] };
        return true;
    }
    return false;
}

bool sc_cli_parse_int(const char *str, int *out) {
    if (str == NULL || str[0] == '\0') {
        return false;
    }
    char *end   = NULL;
    long  value = strtol(str, &end, 10);
    if (*end != '\0' || value < 0 || value > INT_MAX) {
        return false;
    }
    *out = (int)value;
    return true;
}

bool sc_cli_parse_double(const char *str, double *out) {
    if (str == NULL || str[0] == '\0') {
        return false;
    }
    char  *end   = NULL;
    double value = strtod(str, &end);
    if (*end != '\0') {
        return false;
    }
    *out = value;
    return true;
}

static bool lookup_value(const ScCliName *table, size_t count,
                         const char *name, int *out) {
    if (name == NULL) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (strcmp(table[i].name, name) == 0) {
            *out = table[i].value;
            return true;
        }
    }
    return false;
}

static bool parse_hex_color(const char *spec, ScColor *out) {
    enum { HEX_COLOR_DIGITS = 6 };
    if (strlen(spec + 1) != HEX_COLOR_DIGITS) {
        return false;
    }
    char *end   = NULL;
    long  value = strtol(spec + 1, &end, 16);
    if (*end != '\0' || value < 0) {
        return false;
    }
    *out = sc_color_from_rgb((uint8_t)((value >> 16) & 0xff),
                             (uint8_t)((value >> 8) & 0xff),
                             (uint8_t)(value & 0xff));
    return true;
}

static bool parse_rgb_color(const char *spec, ScColor *out) {
    enum { RGB_COMPONENTS = 3, RGB_MAX = 255 };
    int    values[RGB_COMPONENTS] = { 0 };
    size_t count                  = RGB_COMPONENTS;
    if (!parse_int_list(spec, values, &count) || count != RGB_COMPONENTS) {
        return false;
    }
    for (size_t i = 0; i < RGB_COMPONENTS; i++) {
        if (values[i] > RGB_MAX) {
            return false;
        }
    }
    *out = sc_color_from_rgb((uint8_t)values[0], (uint8_t)values[1],
                             (uint8_t)values[2]);
    return true;
}

/* Parses up to `*count` comma-separated non-negative integers; on success
   `*count` is set to the number actually parsed. */
static bool parse_int_list(const char *spec, int *values, size_t *count) {
    size_t      parsed = 0;
    const char *cursor = spec;

    while (*cursor != '\0' && parsed < *count) {
        char *end   = NULL;
        long  value = strtol(cursor, &end, 10);
        if (end == cursor || value < 0 || value > INT_MAX) {
            return false;
        }
        values[parsed] = (int)value;
        parsed++;

        if (*end == '\0') {
            *count = parsed;
            return true;
        }
        if (*end != ',') {
            return false;
        }
        cursor = end + 1;
    }
    return false;
}
