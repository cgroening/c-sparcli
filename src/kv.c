#include "sparcli.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Internal structure ──────────────────────────────────────────────────── */

typedef struct { char *key; char *value; } ScKVEntry;

struct ScKV {
    ScKVOpts   opts;
    ScKVEntry *entries;
    size_t     count;
    size_t     cap;
};

/* zero-initialised ScOptions = "no formatting" sentinel (same as list.c) */
static int opts_has_format(ScOptions o) {
    return o.style != 0 || o.fg.index != 0 || o.fg.r || o.fg.g || o.fg.b
                        || o.bg.index != 0 || o.bg.r || o.bg.g || o.bg.b;
}

/* ── Public API ──────────────────────────────────────────────────────────── */

ScKV *sc_kv_new(ScKVOpts opts) {
    ScKV *kv = calloc(1, sizeof(ScKV));
    kv->opts = opts;
    return kv;
}

void sc_kv_add(ScKV *kv, const char *key, const char *value) {
    if (kv->count == kv->cap) {
        kv->cap = kv->cap ? kv->cap * 2 : 8;
        kv->entries = realloc(kv->entries, kv->cap * sizeof(ScKVEntry));
    }
    kv->entries[kv->count].key   = strdup(key   ? key   : "");
    kv->entries[kv->count].value = strdup(value ? value : "");
    kv->count++;
}

/* ── Rendering ───────────────────────────────────────────────────────────── */

/* Word-wrap text into lines of at most avail visible columns; continuation
   lines are indented by `indent` spaces. */
static void kv_print_wrapped(const char *text, int avail, int indent,
                              ScOptions opts, int fmt) {
    if (!text || !*text) { fputc('\n', stdout); return; }
    if (avail < 1) avail = 1;

    char pad[256];
    int  pad_len = indent < (int)sizeof(pad) - 1 ? indent : (int)sizeof(pad) - 1;
    memset(pad, ' ', (size_t)pad_len);
    pad[pad_len] = '\0';

    const char *p = text;
    int first = 1;

    while (*p) {
        char buf[4096];
        int  blen = 0, bvis = 0;

        while (*p) {
            const char *w = p;
            while (*w && *w != ' ') w++;
            int wbytes = (int)(w - p);
            int wvis   = (int)sc_utf8_vis_w(p, (size_t)wbytes);
            int gap    = (bvis > 0) ? 1 : 0;

            /* flush line if next word won't fit (keep at least one word per line) */
            if (gap > 0 && bvis + gap + wvis > avail) break;

            if (gap) buf[blen++] = ' ';
            memcpy(buf + blen, p, (size_t)wbytes);
            blen += wbytes;
            bvis += gap + wvis;

            p = w;
            while (*p == ' ') p++;
        }

        buf[blen] = '\0';
        if (!first) fputs(pad, stdout);
        first = 0;

        if (fmt) sc_print(buf, opts);
        else     fputs(buf, stdout);
        fputc('\n', stdout);
    }
}

void sc_kv_print(const ScKV *kv) {
    if (!kv || kv->count == 0) return;

    int total_w = kv->opts.width > 0 ? kv->opts.width : sc_term_width();
    int margin  = kv->opts.margin > 0 ? kv->opts.margin : 0;

    /* auto key-column width */
    int key_w = kv->opts.key_width;
    if (key_w <= 0) {
        key_w = 0;
        for (size_t i = 0; i < kv->count; i++) {
            int w = (int)sc_utf8_vis_w(kv->entries[i].key,
                                        strlen(kv->entries[i].key));
            if (w > key_w) key_w = w;
        }
    }

    const char *sep   = kv->opts.sep ? kv->opts.sep : "  ";
    int         sep_w = (int)sc_utf8_vis_w(sep, strlen(sep));

    int avail = total_w - margin - key_w - sep_w - margin;
    if (avail < 1) avail = 1;

    int key_fmt = opts_has_format(kv->opts.key_opts);
    int val_fmt = opts_has_format(kv->opts.val_opts);

    char margin_buf[128] = {0};
    int  margin_len = margin < (int)sizeof(margin_buf) - 1
                      ? margin : (int)sizeof(margin_buf) - 1;
    memset(margin_buf, ' ', (size_t)margin_len);
    margin_buf[margin_len] = '\0';

    for (size_t i = 0; i < kv->count; i++) {
        const char *key = kv->entries[i].key;
        const char *val = kv->entries[i].value;

        fputs(margin_buf, stdout);

        /* print key, padded to key_w */
        int natural_kw = (int)sc_utf8_vis_w(key, strlen(key));
        int kbytes = (natural_kw > key_w)
                     ? (int)sc_utf8_trim_to_cols(key, key_w)
                     : (int)strlen(key);
        int kprinted = natural_kw < key_w ? natural_kw : key_w;

        if (key_fmt) {
            char tmp[512];
            if (kbytes < (int)sizeof(tmp) - 1) {
                memcpy(tmp, key, (size_t)kbytes);
                tmp[kbytes] = '\0';
                sc_print(tmp, kv->opts.key_opts);
            }
        } else {
            fwrite(key, 1, (size_t)kbytes, stdout);
        }
        for (int s = kprinted; s < key_w; s++) fputc(' ', stdout);

        fputs(sep, stdout);

        /* print value */
        int continuation = margin_len + key_w + sep_w;
        if (kv->opts.wrap_val) {
            kv_print_wrapped(val, avail, continuation, kv->opts.val_opts, val_fmt);
        } else {
            int vbytes = (int)sc_utf8_trim_to_cols(val, avail);
            if (val_fmt) {
                char tmp[512];
                if (vbytes < (int)sizeof(tmp) - 1) {
                    memcpy(tmp, val, (size_t)vbytes);
                    tmp[vbytes] = '\0';
                    sc_print(tmp, kv->opts.val_opts);
                }
            } else {
                fwrite(val, 1, (size_t)vbytes, stdout);
            }
            fputc('\n', stdout);
        }

        if (kv->opts.item_gap > 0 && i + 1 < kv->count)
            for (int g = 0; g < kv->opts.item_gap; g++) fputc('\n', stdout);
    }
}

/* ── Memory management ───────────────────────────────────────────────────── */

void sc_kv_free(ScKV *kv) {
    if (!kv) return;
    for (size_t i = 0; i < kv->count; i++) {
        free(kv->entries[i].key);
        free(kv->entries[i].value);
    }
    free(kv->entries);
    free(kv);
}
