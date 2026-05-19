#include "sparcli.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Helpers ─────────────────────────────────────────────────────────────── */

/* zero-initialised ScOptions = "no formatting" sentinel (same as list.c / kv.c) */
static int opts_has_format(ScOptions o) {
    return o.style != 0 || o.fg.index != 0 || o.fg.r || o.fg.g || o.fg.b
                        || o.bg.index != 0 || o.bg.r || o.bg.g || o.bg.b;
}

/* Build the composed badge string lcap+pad+text+pad+rcap into buf (size >= total).
   Returns the byte length written (not including NUL). */
static size_t badge_build(char *buf, const char *lcap, const char *rcap,
                           const char *text, int pad) {
    size_t lcap_len = strlen(lcap);
    size_t rcap_len = strlen(rcap);
    size_t text_len = text ? strlen(text) : 0;

    size_t pos = 0;
    memcpy(buf + pos, lcap, lcap_len); pos += lcap_len;
    for (int i = 0; i < pad; i++) buf[pos++] = ' ';
    if (text) { memcpy(buf + pos, text, text_len); pos += text_len; }
    for (int i = 0; i < pad; i++) buf[pos++] = ' ';
    memcpy(buf + pos, rcap, rcap_len); pos += rcap_len;
    buf[pos] = '\0';
    return pos;
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void sc_print_badge(const char *text, ScBadgeOpts opts) {
    const char *lcap = opts.left_cap  ? opts.left_cap  : "[";
    const char *rcap = opts.right_cap ? opts.right_cap : "]";
    int         pad  = opts.pad > 0   ? opts.pad       : 0;

    size_t total = strlen(lcap) + (size_t)pad + (text ? strlen(text) : 0)
                 + (size_t)pad + strlen(rcap) + 1;

    char  stack[512];
    char *buf = (total <= sizeof(stack)) ? stack : malloc(total);
    if (!buf) return;

    badge_build(buf, lcap, rcap, text, pad);

    if (opts_has_format(opts.text_opts)) {
        sc_apply_colors(opts.text_opts.fg, opts.text_opts.bg);
        fputs(buf, stdout);
        fputs("\033[0m", stdout);
    } else {
        fputs(buf, stdout);
    }

    if (buf != stack) free(buf);
}

void sc_text_append_badge(ScText *t, const char *text, ScBadgeOpts opts) {
    const char *lcap = opts.left_cap  ? opts.left_cap  : "[";
    const char *rcap = opts.right_cap ? opts.right_cap : "]";
    int         pad  = opts.pad > 0   ? opts.pad       : 0;

    size_t total = strlen(lcap) + (size_t)pad + (text ? strlen(text) : 0)
                 + (size_t)pad + strlen(rcap) + 1;

    char  stack[512];
    char *buf = (total <= sizeof(stack)) ? stack : malloc(total);
    if (!buf) return;

    badge_build(buf, lcap, rcap, text, pad);
    sc_text_append(t, buf, opts.text_opts);

    if (buf != stack) free(buf);
}
