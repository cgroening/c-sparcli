#include "sparcli.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** Default left cap when `opts.left_cap` is `NULL`. */
#define DEFAULT_LEFT_CAP "["

/** Default right cap when `opts.right_cap` is `NULL`. */
#define DEFAULT_RIGHT_CAP "]"

/**
 * Stack buffer size for the composed badge string; allocations happen only
 * when the badge exceeds this length.
 */
#define STACK_BUFFER_SIZE 512


/** Resolved badge inputs for one call. */
typedef struct Badge {
    /** Badge text; may be `NULL`. */
    const char *text;

    /** Resolved left cap (never `NULL`). */
    const char *left_cap;

    /** Resolved right cap (never `NULL`). */
    const char *right_cap;

    /** Spaces inserted inside each cap; clamped to `>= 0`. */
    int pad;

    /** Style applied to the badge as a whole. */
    ScTextStyle style;
} Badge;


// Forward declarations indented to reflect call hierarchy
static Badge resolve_badge(const char *text, ScBadgeOpts opts);
static size_t compute_buffer_size(const Badge *self);
static size_t compose_badge_string(const Badge *self, char *buffer);
static bool style_has_format(ScTextStyle style);


void sc_print_badge(const char *text, ScBadgeOpts opts) {
    Badge self = resolve_badge(text, opts);
    size_t buffer_size = compute_buffer_size(&self);

    char stack_buffer[STACK_BUFFER_SIZE];
    char *buffer = (buffer_size <= sizeof(stack_buffer))
        ? stack_buffer : malloc(buffer_size);
    if (!buffer) { return; }

    compose_badge_string(&self, buffer);

    if (style_has_format(self.style)) {
        sc_apply_colors(self.style.fg, self.style.bg);
        fputs(buffer, sc_output_stream());
        fputs(SC_ANSI_ESCAPE_CODE_RESET, sc_output_stream());
    } else {
        fputs(buffer, sc_output_stream());
    }

    if (buffer != stack_buffer) { free(buffer); }
}

void sc_text_append_badge(
    ScText *text_obj, const char *text, ScBadgeOpts opts
) {
    Badge self = resolve_badge(text, opts);
    size_t buffer_size = compute_buffer_size(&self);

    char stack_buffer[STACK_BUFFER_SIZE];
    char *buffer = (buffer_size <= sizeof(stack_buffer))
        ? stack_buffer : malloc(buffer_size);
    if (!buffer) { return; }

    compose_badge_string(&self, buffer);
    sc_text_append(text_obj, buffer, self.style);

    if (buffer != stack_buffer) { free(buffer); }
}


/** Resolves caps and pad defaults into a `Badge`. */
static Badge resolve_badge(const char *text, ScBadgeOpts opts) {
    return (Badge){
        .text = text,
        .left_cap = opts.left_cap ? opts.left_cap : DEFAULT_LEFT_CAP,
        .right_cap = opts.right_cap ? opts.right_cap : DEFAULT_RIGHT_CAP,
        .pad = opts.pad > 0 ? opts.pad : 0,
        .style = opts.text_style,
    };
}

/** Returns the buffer size needed for the composed string (including `\\0`). */
static size_t compute_buffer_size(const Badge *self) {
    size_t left_length = strlen(self->left_cap);
    size_t right_length = strlen(self->right_cap);
    size_t text_length = self->text ? strlen(self->text) : 0;
    return left_length + (size_t)self->pad + text_length
        + (size_t)self->pad + right_length + 1;
}

/**
 * Writes `left_cap + pad×' ' + text + pad×' ' + right_cap + '\\0'` into
 * `buffer`. Returns the byte length written (excluding the terminator).
 */
static size_t compose_badge_string(const Badge *self, char *buffer) {
    size_t left_length = strlen(self->left_cap);
    size_t right_length = strlen(self->right_cap);
    size_t text_length = self->text ? strlen(self->text) : 0;

    size_t position = 0;
    memcpy(buffer + position, self->left_cap, left_length);
    position += left_length;

    for (int i = 0; i < self->pad; i++) { buffer[position++] = ' '; }

    if (self->text) {
        memcpy(buffer + position, self->text, text_length);
        position += text_length;
    }

    for (int i = 0; i < self->pad; i++) { buffer[position++] = ' '; }

    memcpy(buffer + position, self->right_cap, right_length);
    position += right_length;

    buffer[position] = '\0';
    return position;
}

/**
 * Returns `true` when `style` carries any formatting; zero-initialized
 * `ScTextStyle` returns `false`.
 */
static bool style_has_format(ScTextStyle style) {
    return style.attr != 0
        || style.fg.index != 0 || style.fg.r || style.fg.g || style.fg.b
        || style.bg.index != 0 || style.bg.r || style.bg.g || style.bg.b;
}
