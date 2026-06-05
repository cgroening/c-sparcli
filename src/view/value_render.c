#include "view/sparcli_value_render.h"
#include "output/sparcli_capture.h"
#include "core/sparcli_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** Resolved colors + settings for one render pass. */
typedef struct {
    int     step;
    bool    sort;
    bool    color;
    ScColor key, str, num, lit;
} Ctx;

static ScColor or_color(ScColor c, ScColor def) {
    return c.index != 0 ? c : def;
}

/** Appends `s` either styled with `fg` or plain (when colors are off). */
static void app(ScText *t, const char *s, ScColor fg, const Ctx *c) {
    ScTextStyle st = c->color
        ? (ScTextStyle){ SC_TEXT_ATTR_NONE, fg, SC_ANSI_COLOR_NONE }
        : (ScTextStyle){ 0 };
    sc_text_append(t, s, st);
}

/** Appends `n` spaces of indentation. */
static void indent(ScText *t, int n) {
    char buf[64];
    while (n > 0) {
        int chunk = n < (int)sizeof buf - 1 ? n : (int)sizeof buf - 1;
        memset(buf, ' ', (size_t)chunk);
        buf[chunk] = '\0';
        sc_text_append(t, buf, (ScTextStyle){ 0 });
        n -= chunk;
    }
}

/** Appends a `"..."`-quoted string span (no full JSON escaping; display). */
static void app_quoted(ScText *t, const char *s, ScColor fg, const Ctx *c) {
    app(t, "\"", fg, c);
    app(t, s ? s : "", fg, c);
    app(t, "\"", fg, c);
}

/** Comparator for sorted object keys. */
static int key_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void render(ScText *t, const ScValue *v, int depth, const Ctx *c);

/** Renders an object's members (already know `v` is an object). */
static void render_object(ScText *t, const ScValue *v, int depth,
                          const Ctx *c) {
    size_t n = sc_value_len(v);
    if (n == 0) { app(t, "{}", SC_ANSI_COLOR_NONE, c); return; }

    app(t, "{\n", SC_ANSI_COLOR_NONE, c);

    /* Optionally collect + sort keys (fall back to direct lookup on OOM). */
    const char **keys = malloc(n * sizeof *keys);
    if (keys) {
        for (size_t i = 0; i < n; i++) { keys[i] = sc_value_key_at(v, i); }
        if (c->sort) { qsort(keys, n, sizeof *keys, key_cmp); }
    }

    for (size_t i = 0; i < n; i++) {
        const char *k = keys ? keys[i] : sc_value_key_at(v, i);
        indent(t, (depth + 1) * c->step);
        app_quoted(t, k, c->key, c);
        app(t, ": ", SC_ANSI_COLOR_NONE, c);
        render(t, sc_value_get(v, k), depth + 1, c);
        app(t, i + 1 < n ? ",\n" : "\n", SC_ANSI_COLOR_NONE, c);
    }
    free(keys);

    indent(t, depth * c->step);
    app(t, "}", SC_ANSI_COLOR_NONE, c);
}

/** Renders an array's elements. */
static void render_array(ScText *t, const ScValue *v, int depth,
                         const Ctx *c) {
    size_t n = sc_value_len(v);
    if (n == 0) { app(t, "[]", SC_ANSI_COLOR_NONE, c); return; }

    app(t, "[\n", SC_ANSI_COLOR_NONE, c);
    for (size_t i = 0; i < n; i++) {
        indent(t, (depth + 1) * c->step);
        render(t, sc_value_at(v, i), depth + 1, c);
        app(t, i + 1 < n ? ",\n" : "\n", SC_ANSI_COLOR_NONE, c);
    }
    indent(t, depth * c->step);
    app(t, "]", SC_ANSI_COLOR_NONE, c);
}

/** Renders one value node. */
static void render(ScText *t, const ScValue *v, int depth, const Ctx *c) {
    char buf[64];
    switch (sc_value_type(v)) {
        case SC_VALUE_NULL:
            app(t, "null", c->lit, c);
            break;
        case SC_VALUE_BOOL: {
            bool b = false;
            sc_value_as_bool(v, &b);
            app(t, b ? "true" : "false", c->lit, c);
            break;
        }
        case SC_VALUE_INT: {
            int64_t iv = 0;
            sc_value_as_int(v, &iv);
            snprintf(buf, sizeof buf, "%lld", (long long)iv);
            app(t, buf, c->num, c);
            break;
        }
        case SC_VALUE_FLOAT: {
            double dv = 0;
            sc_value_as_float(v, &dv);
            snprintf(buf, sizeof buf, "%g", dv);
            app(t, buf, c->num, c);
            break;
        }
        case SC_VALUE_STRING:
        case SC_VALUE_DATETIME:
            app_quoted(t, sc_value_as_string(v), c->str, c);
            break;
        case SC_VALUE_ARRAY:
            render_array(t, v, depth, c);
            break;
        case SC_VALUE_OBJECT:
            render_object(t, v, depth, c);
            break;
    }
}

ScText *sc_value_render_text(const ScValue *value, ScValueRenderOpts opts) {
    Ctx c = {
        .step  = opts.indent > 0 ? opts.indent : 2,
        .sort  = opts.sort_keys,
        .color = !opts.no_color,
        .key   = or_color(opts.key_color, SC_ANSI_COLOR_BLUE),
        .str   = or_color(opts.string_color, SC_ANSI_COLOR_GREEN),
        .num   = or_color(opts.number_color, SC_ANSI_COLOR_CYAN),
        .lit   = or_color(opts.literal_color, SC_ANSI_COLOR_MAGENTA),
    };
    ScText *t = sc_text_new();
    if (!t) { return NULL; }
    render(t, value, 0, &c);
    return t;
}

void sc_value_render(const ScValue *value, ScValueRenderOpts opts) {
    ScText *t = sc_value_render_text(value, opts);
    sc_print_text(t);
    sc_println("", (ScTextStyle){ 0 });
    sc_text_free(t);
}

ScRendered *sc_capture_value(const ScValue *value, ScValueRenderOpts opts) {
    ScText *t = sc_value_render_text(value, opts);
    ScRendered *r = sc_capture_text(t);
    sc_text_free(t);
    return r;
}
