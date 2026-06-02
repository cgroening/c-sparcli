#include "sparcli.h"
#include "args/args_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** Number of components in an `R,G,B` color value. */
#define COLOR_RGB_COMPONENTS 3

/** Maximum value of one RGB channel. */
#define COLOR_CHANNEL_MAX 255

/** Number of hex digits in a `#RRGGBB` color value. */
#define COLOR_HEX_DIGITS 6


/** Named ANSI colors accepted by `SC_ARG_COLOR` values. */
static const struct {
    const char *name;
    int         index;
} COLOR_NAMES[] = {
    { "black",   1 },
    { "red",     2 },
    { "green",   3 },
    { "yellow",  4 },
    { "blue",    5 },
    { "magenta", 6 },
    { "cyan",    7 },
    { "white",   8 },
};

/** Number of entries in `COLOR_NAMES`. */
#define COLOR_NAME_COUNT (sizeof COLOR_NAMES / sizeof COLOR_NAMES[0])


// Forward declarations indented to reflect call hierarchy
static bool parse_hex_color(const char *text, ScColor *out);
static bool parse_rgb_color(const char *text, ScColor *out);
static void describe_choices(
    const ArgOption *option, char *reason, size_t reason_size
);


bool sc_args_parse_int(const char *text, long *out) {
    if (!text || text[0] == '\0') { return false; }

    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (*end != '\0') { return false; }
    *out = value;
    return true;
}

bool sc_args_parse_double(const char *text, double *out) {
    if (!text || text[0] == '\0') { return false; }

    char *end = NULL;
    double value = strtod(text, &end);
    if (*end != '\0') { return false; }
    *out = value;
    return true;
}

bool sc_args_parse_color(const char *text, ScColor *out) {
    if (!text || text[0] == '\0') { return false; }

    for (size_t i = 0; i < COLOR_NAME_COUNT; i++) {
        if (strcmp(COLOR_NAMES[i].name, text) == 0) {
            *out = (ScColor){ .index = COLOR_NAMES[i].index };
            return true;
        }
    }
    if (text[0] == '#') {
        return parse_hex_color(text, out);
    }
    return parse_rgb_color(text, out);
}

int sc_args_find_choice(const ArgOption *option, const char *text) {
    if (!option || !text || option->choice_count == 0) { return -1; }

    for (size_t i = 0; i < option->choice_count; i++) {
        if (strcmp(option->choices[i], text) == 0) {
            return (int)i;
        }
    }
    return -1;
}

bool sc_args_validate_value(
    const ArgOption *option, const char *text,
    char *reason, size_t reason_size
) {
    if (reason_size > 0) { reason[0] = '\0'; }

    // Choices restrict any type to the configured set
    if (option->choice_count > 0) {
        if (sc_args_find_choice(option, text) >= 0) { return true; }
        describe_choices(option, reason, reason_size);
        return false;
    }

    long int_value = 0;
    double double_value = 0.0;
    ScColor color_value = { 0 };
    switch (option->type) {
        case SC_ARG_INT:
            if (sc_args_parse_int(text, &int_value)) { return true; }
            snprintf(reason, reason_size, "expected an integer");
            return false;
        case SC_ARG_DOUBLE:
            if (sc_args_parse_double(text, &double_value)) { return true; }
            snprintf(reason, reason_size, "expected a number");
            return false;
        case SC_ARG_COLOR:
            if (sc_args_parse_color(text, &color_value)) { return true; }
            snprintf(
                reason, reason_size,
                "expected a color name, #RRGGBB or R,G,B"
            );
            return false;
        case SC_ARG_STR:
            return true;
    }
    return true;
}

/** Parses `#RRGGBB` into an RGB color. */
static bool parse_hex_color(const char *text, ScColor *out) {
    if (strlen(text + 1) != COLOR_HEX_DIGITS) { return false; }

    char *end = NULL;
    long value = strtol(text + 1, &end, 16);
    if (*end != '\0' || value < 0) { return false; }

    *out = sc_color_from_rgb(
        (uint8_t)((value >> 16) & 0xff),
        (uint8_t)((value >> 8) & 0xff),
        (uint8_t)(value & 0xff)
    );
    return true;
}

/** Parses `R,G,B` (decimal, 0-255 each) into an RGB color. */
static bool parse_rgb_color(const char *text, ScColor *out) {
    int channels[COLOR_RGB_COMPONENTS] = { 0 };
    const char *cursor = text;

    for (size_t i = 0; i < COLOR_RGB_COMPONENTS; i++) {
        char *end = NULL;
        long value = strtol(cursor, &end, 10);
        if (end == cursor || value < 0 || value > COLOR_CHANNEL_MAX) {
            return false;
        }
        channels[i] = (int)value;
        cursor = end;

        bool is_last_channel = i == COLOR_RGB_COMPONENTS - 1;
        if (is_last_channel) {
            if (*cursor != '\0') { return false; }
        } else {
            if (*cursor != ',') { return false; }
            cursor++;
        }
    }

    *out = sc_color_from_rgb(
        (uint8_t)channels[0], (uint8_t)channels[1], (uint8_t)channels[2]
    );
    return true;
}

/** Writes "valid choices: a, b, c" into `reason`. */
static void describe_choices(
    const ArgOption *option, char *reason, size_t reason_size
) {
    size_t used = (size_t)snprintf(reason, reason_size, "valid choices: ");
    for (size_t i = 0; i < option->choice_count && used < reason_size; i++) {
        used += (size_t)snprintf(
            reason + used, reason_size - used, "%s%s",
            i > 0 ? ", " : "", option->choices[i]
        );
    }
}
