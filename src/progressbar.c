#include "sparcli.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Character tables ────────────────────────────────────────────────────── */

static const struct {
    const char *fill;   /* repeated fill character                    */
    const char *head;   /* edge/transition char; NULL = same as fill  */
    const char *empty;  /* repeated empty character                   */
} pc[] = {
    [SC_PROGRESS_BLOCK]  = { "\xe2\x96\x88", NULL,              "\xe2\x96\x91" }, /* █ ░ */
    [SC_PROGRESS_ASCII]  = { "=",            ">",               " "            },
    [SC_PROGRESS_LINE]   = { "\xe2\x94\x81", NULL,              "\xe2\x95\x8c" }, /* ━ ╌ */
    [SC_PROGRESS_SHADED] = { "\xe2\x96\x93", "\xe2\x96\x92",   "\xe2\x96\x91" }, /* ▓ ▒ ░ */
};

/* ── Internal structure ──────────────────────────────────────────────────── */

struct ScProgressBar {
    ScProgressBarOpts  opts;
    char              *label;  /* owned */
};

/* zero-initialised ScOptions = "no formatting" sentinel (same as list.c) */
static int opts_no_fmt(ScOptions o) {
    return o.style == 0 &&
           o.fg.index == 0 && !o.fg.r && !o.fg.g && !o.fg.b &&
           o.bg.index == 0 && !o.bg.r && !o.bg.g && !o.bg.b;
}

/* ── Helpers ─────────────────────────────────────────────────────────────── */

/* zero-initialised ScColor (index=0, rgb=0) = SC_COLOR_BLACK from struct init,
   treated as "no color requested"; use sc_rgb(0,0,0) for explicit black */
static int color_is_set(ScColor c) {
    return c.index != -2 && !(c.index == 0 && !c.r && !c.g && !c.b);
}

static void print_n(const char *ch, int n, ScColor col) {
    if (n <= 0) return;
    int colored = color_is_set(col);
    if (colored) sc_apply_colors(col, SC_COLOR_NONE);
    for (int i = 0; i < n; i++) fputs(ch, stdout);
    if (colored) fputs("\033[0m", stdout);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

ScProgressBar *sc_progressbar_new(ScProgressBarOpts opts) {
    ScProgressBar *b = calloc(1, sizeof(ScProgressBar));
    b->opts = opts;
    return b;
}

void sc_progressbar_set_label(ScProgressBar *b, const char *label) {
    free(b->label);
    b->label = label ? strdup(label) : NULL;
}

/* ── Rendering ───────────────────────────────────────────────────────────── */

static void render_bar(ScProgressBar *b, double value, double max, int final) {
    double ratio = (max > 0.0) ? value / max : value;
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;

    /* ── caps ── */
    const char *lcap  = b->opts.left_cap;
    const char *rcap  = b->opts.right_cap;
    int         lcap_w = lcap ? (int)sc_utf8_vis_w(lcap, strlen(lcap)) : 0;
    int         rcap_w = rcap ? (int)sc_utf8_vis_w(rcap, strlen(rcap)) : 0;

    /* ── percent string ── */
    char pct_buf[16] = "";
    if (b->opts.show_percent)
        snprintf(pct_buf, sizeof(pct_buf), " %3.0f%%", ratio * 100.0);
    int pct_w = (int)sc_utf8_vis_w(pct_buf, strlen(pct_buf));

    /* ── value string ── */
    char val_buf[64]  = "";
    int  val_reserve  = 0;
    if (b->opts.show_value && max > 0.0) {
        /* reserve width for worst case (value == max) to keep bar stable */
        char max_buf[64];
        long lv = (long)value, lm = (long)max;
        if ((double)lv == value && (double)lm == max) {
            snprintf(val_buf,  sizeof(val_buf),  " (%ld/%ld)", lv, lm);
            snprintf(max_buf,  sizeof(max_buf),  " (%ld/%ld)", lm, lm);
        } else {
            snprintf(val_buf,  sizeof(val_buf),  " (%.1f/%.1f)", value, max);
            snprintf(max_buf,  sizeof(max_buf),  " (%.1f/%.1f)", max,   max);
        }
        val_reserve = (int)sc_utf8_vis_w(max_buf, strlen(max_buf));
    }

    /* ── label field ── */
    int label_field_w = 0;
    if (b->label) {
        int lw = (int)sc_utf8_vis_w(b->label, strlen(b->label));
        label_field_w = (b->opts.label_width > 0 ? b->opts.label_width : lw) + 2;
    }

    /* ── bar width ── */
    int total_w = b->opts.width > 0 ? b->opts.width : sc_term_width();
    int bw = b->opts.bar_width > 0
        ? b->opts.bar_width
        : total_w - label_field_w - lcap_w - rcap_w - pct_w - val_reserve;
    if (bw < 1) bw = 1;

    /* ── fill color (with optional thresholds) ── */
    ScColor fill_col  = b->opts.fill_color;
    if (b->opts.use_thresholds) {
        double tmid  = b->opts.threshold_mid  > 0.0 ? b->opts.threshold_mid  : 0.5;
        double thigh = b->opts.threshold_high > 0.0 ? b->opts.threshold_high : 0.75;
        fill_col = b->opts.color_low;
        if (ratio >= tmid)  fill_col = b->opts.color_mid;
        if (ratio >= thigh) fill_col = b->opts.color_high;
    }
    ScColor empty_col = b->opts.empty_color;

    int filled = (int)(ratio * bw);
    if (filled > bw) filled = bw;
    int style = (int)b->opts.style;

    /* ── print label ── */
    if (b->label) {
        int natural_w = (int)sc_utf8_vis_w(b->label, strlen(b->label));
        int field_w   = b->opts.label_width > 0 ? b->opts.label_width : natural_w;
        int bytes     = (natural_w > field_w)
                        ? (int)sc_utf8_trim_to_cols(b->label, field_w) : (int)strlen(b->label);
        int printed_w = natural_w < field_w ? natural_w : field_w;

        if (!opts_no_fmt(b->opts.label_opts)) {
            char tmp[512];
            if (bytes < (int)sizeof(tmp) - 1) {
                memcpy(tmp, b->label, (size_t)bytes);
                tmp[bytes] = '\0';
                sc_print(tmp, b->opts.label_opts);
            }
        } else {
            fwrite(b->label, 1, (size_t)bytes, stdout);
        }
        for (int i = printed_w; i < field_w; i++) fputc(' ', stdout);
        fputs("  ", stdout);
    }

    /* ── left cap ── */
    if (lcap) fputs(lcap, stdout);

    /* ── bar body ── */
    const char *fill_ch  = pc[style].fill;
    const char *head_ch  = pc[style].head ? pc[style].head : pc[style].fill;
    const char *empty_ch = pc[style].empty;

    if (style == SC_PROGRESS_ASCII || style == SC_PROGRESS_SHADED) {
        int has_head = (filled > 0 && filled < bw) ? 1 : 0;
        int solid    = filled - has_head;
        int empty_n  = bw - filled;
        print_n(fill_ch, solid, fill_col);
        if (has_head) {
            int colored = color_is_set(fill_col);
            if (colored) sc_apply_colors(fill_col, SC_COLOR_NONE);
            fputs(head_ch, stdout);
            if (colored) fputs("\033[0m", stdout);
        }
        print_n(empty_ch, empty_n, empty_col);
    } else {
        print_n(fill_ch, filled,       fill_col);
        print_n(empty_ch, bw - filled, empty_col);
    }

    /* ── right cap ── */
    if (rcap) fputs(rcap, stdout);

    /* ── percent ── */
    if (b->opts.show_percent) fputs(pct_buf, stdout);

    /* ── value ── */
    if (b->opts.show_value && max > 0.0) {
        fputs(val_buf, stdout);
        int actual_w = (int)sc_utf8_vis_w(val_buf, strlen(val_buf));
        for (int i = actual_w; i < val_reserve; i++) fputc(' ', stdout);
    }

    if (final) {
        fputc('\n', stdout);
    } else {
        fputc('\r', stdout);
        fflush(stdout);
    }
}

void sc_progressbar_draw(ScProgressBar *b, double value, double max) {
    render_bar(b, value, max, 0);
}

void sc_progressbar_finish(ScProgressBar *b, double value, double max) {
    render_bar(b, value, max, 1);
}

/* ── Memory management ───────────────────────────────────────────────────── */

void sc_progressbar_free(ScProgressBar *b) {
    if (!b) return;
    free(b->label);
    free(b);
}
