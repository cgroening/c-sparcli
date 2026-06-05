#include "sparcli.h"
#include "output/sparcli_multiprogress.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct ScMultiProgress {
    ScLive         *live;
    ScProgressBar **bars;
    double         *values;
    double         *maxes;
    size_t          count;
    size_t          cap;
};


/**
 * Renders one bar into a heap string with the trailing `\r`/`\n` stripped,
 * by temporarily pointing the output stream at a memory stream. The live
 * session keeps writing to the stream it captured at begin, so this redirect
 * does not disturb it. Returns NULL on failure.
 */
static char *capture_bar_line(ScProgressBar *bar, double value, double max) {
    char *buf = NULL;
    size_t size = 0;
    FILE *ms = open_memstream(&buf, &size);
    if (!ms) { return NULL; }

    FILE *prev = sc_output_stream();
    sc_output_set_stream(ms);
    sc_progressbar_draw(bar, value, max);
    sc_output_set_stream(prev);
    fclose(ms);

    if (!buf) { return NULL; }
    size_t n = size;
    while (n > 0 && (buf[n - 1] == '\r' || buf[n - 1] == '\n')) {
        buf[--n] = '\0';
    }
    return buf;
}

/** Builds the stacked frame (one line per bar). Caller frees with
 *  sc_rendered_free. */
static ScRendered *build_frame(ScMultiProgress *mp) {
    ScRendered *r = calloc(1, sizeof *r);
    if (!r) { return NULL; }
    if (mp->count == 0) { return r; }

    r->lines = calloc(mp->count, sizeof *r->lines);
    r->column_widths = calloc(mp->count, sizeof *r->column_widths);
    if (!r->lines || !r->column_widths) {
        sc_rendered_free(r);
        return NULL;
    }
    r->line_count = mp->count;

    int maxw = 0;
    for (size_t i = 0; i < mp->count; i++) {
        char *line = capture_bar_line(mp->bars[i], mp->values[i], mp->maxes[i]);
        r->lines[i] = line ? line : calloc(1, 1);
        int w = (int)sc_utf8_string_length(r->lines[i], strlen(r->lines[i]));
        r->column_widths[i] = w;
        if (w > maxw) { maxw = w; }
    }
    r->max_column_width = maxw;
    return r;
}

/** Recomputes and pushes the current stack to the live display. */
static void redraw(ScMultiProgress *mp) {
    ScRendered *frame = build_frame(mp);
    if (!frame) { return; }
    sc_live_update(mp->live, frame);
    sc_rendered_free(frame);
}


ScMultiProgress *sc_multiprogress_begin(ScMultiProgressOpts opts) {
    ScMultiProgress *mp = calloc(1, sizeof *mp);
    if (!mp) { return NULL; }
    mp->live = sc_live_begin((ScLiveOpts){
        .transient = opts.transient,
        .always = opts.always,
    });
    if (!mp->live) { free(mp); return NULL; }
    return mp;
}

int sc_multiprogress_add(ScMultiProgress *mp, const char *label,
                         ScProgressBarOpts opts) {
    if (!mp) { return -1; }
    if (mp->count == mp->cap) {
        size_t nc = mp->cap ? mp->cap * 2 : 4;
        ScProgressBar **nb = realloc(mp->bars, nc * sizeof *nb);
        double *nv = realloc(mp->values, nc * sizeof *nv);
        double *nm = realloc(mp->maxes, nc * sizeof *nm);
        if (!nb || !nv || !nm) {
            /* keep whatever grew; the originals are still valid */
            mp->bars = nb ? nb : mp->bars;
            mp->values = nv ? nv : mp->values;
            mp->maxes = nm ? nm : mp->maxes;
            return -1;
        }
        mp->bars = nb; mp->values = nv; mp->maxes = nm; mp->cap = nc;
    }

    ScProgressBar *bar = sc_progressbar_new(opts);
    if (!bar) { return -1; }
    if (label) { sc_progressbar_set_label(bar, label); }

    int index = (int)mp->count;
    mp->bars[mp->count] = bar;
    mp->values[mp->count] = 0.0;
    mp->maxes[mp->count] = 0.0;
    mp->count++;
    redraw(mp);
    return index;
}

void sc_multiprogress_update(ScMultiProgress *mp, int index,
                             double value, double max) {
    if (!mp || index < 0 || (size_t)index >= mp->count) { return; }
    mp->values[index] = value;
    mp->maxes[index] = max;
    redraw(mp);
}

void sc_multiprogress_set_label(ScMultiProgress *mp, int index,
                                const char *label) {
    if (!mp || index < 0 || (size_t)index >= mp->count) { return; }
    sc_progressbar_set_label(mp->bars[index], label);
    redraw(mp);
}

void sc_multiprogress_end(ScMultiProgress *mp) {
    if (!mp) { return; }
    sc_live_end(mp->live);
    for (size_t i = 0; i < mp->count; i++) {
        sc_progressbar_free(mp->bars[i]);
    }
    free(mp->bars);
    free(mp->values);
    free(mp->maxes);
    free(mp);
}
