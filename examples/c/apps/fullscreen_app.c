/*
 * fullscreen_app.c
 *
 * A full-screen (alternate-screen) app shell - minimal app code:
 *
 *   - sc_altscreen_begin()/end() owns the alternate screen for the whole loop
 *     (no flicker when switching widgets).
 *   - The widgets run with `fullscreen = true` + a pinned `header` (a captured
 *     panel) + `valign`: each frame they compose [valign-pad][header][body] and
 *     fill the terminal. The FUZZY finder grows with its items up to the screen
 *     height, then scrolls (right-edge scrollbar); the leftover space is placed
 *     by VALIGN (below the finder for TOP, above the header for BOTTOM, or
 *     both for MIDDLE).
 *   - Enter (or ^F) on an item opens the FORM to edit it; ^R adds an item live;
 *     esc quits.
 *
 * Build + run in a real terminal:
 *   make run-example EX=c/apps/fullscreen_app
 */

#include "sparcli.h"

#include <stdio.h>
#include <string.h>


/* ── Knobs you can change ────────────────────────────────────── */

/* Vertical alignment of the whole header+content block (SC_VALIGN_TOP / MIDDLE
   / BOTTOM): fills below the finder (TOP), above the header (BOTTOM) or both
   (MIDDLE). Visible while the finder is shorter than the screen. */
static const ScVAlign VALIGN = SC_VALIGN_TOP;

/* true = the YYY legend is a line BELOW the header panel;
   false = it sits on the panel's bottom border. */
static const bool SUBTITLE_BELOW = true;

/* ────────────────────────────────────────────────────────────── */

/* Shorthand: a foreground text style (no background). */
#define FG(attr, color) ((ScTextStyle){ (attr), (color), SC_ANSI_COLOR_NONE })

enum { ACT_FORM = 1, ACT_REFRESH = 2 };   /* shortcut ids */

/* Context handed to the refresh callback so it can grow the live finder. */
typedef struct { ScFuzzy *fuzzy; int next; } Ctx;

/* CALLBACK shortcut: append a new item and re-filter in place (stays open). */
static bool on_refresh(int id, void *user) {
    (void)id;
    Ctx *ctx = user;
    char label[32];
    snprintf(label, sizeof label, "Task %d (added)", ctx->next++);
    sc_fuzzy_add(ctx->fuzzy, label);
    sc_fuzzy_refresh(ctx->fuzzy);
    return true;   /* keep the finder open */
}

/* The "Fullscreen Example - XXX" content line, in mixed colors/styles. */
static ScText *title_line(bool finder, int count, const char *selected) {
    ScText *t = sc_text_new();
    sc_text_append(t, "Fullscreen Example",
                   FG(SC_TEXT_ATTR_BOLD, SC_COLOR_ACCENT));
    sc_text_append(t, " - ", FG(SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE));
    if (finder) {
        sc_text_append(t, "finder", FG(SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN));
        char c[32];
        snprintf(c, sizeof c, "  %d tasks", count);
        sc_text_append(t, c, FG(SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_CYAN));
    } else {
        sc_text_append(t, "editing ",
                       FG(SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_YELLOW));
        sc_text_append(t, selected, FG(SC_TEXT_ATTR_BOLD, SC_COLOR_ACCENT));
    }
    return t;
}

/* The YYY legend line, in mixed colors/styles. */
static ScText *legend_line(bool finder) {
    ScText *t = sc_text_new();
    ScTextStyle dim = FG(SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE);
    ScTextStyle key = FG(SC_TEXT_ATTR_BOLD, SC_COLOR_ACCENT);
    ScTextStyle red = FG(SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_RED);
    if (finder) {
        sc_text_append(t, "enter ", key); sc_text_append(t, "edit", dim);
        sc_text_append(t, "  \xc2\xb7  ", dim);
        sc_text_append(t, "^R ", key);    sc_text_append(t, "refresh", dim);
        sc_text_append(t, "  \xc2\xb7  ", dim);
        sc_text_append(t, "esc ", red);   sc_text_append(t, "quit", dim);
    } else {
        sc_text_append(t, "^D ", key);  sc_text_append(t, "save", dim);
        sc_text_append(t, "  \xc2\xb7  ", dim);
        sc_text_append(t, "esc ", red); sc_text_append(t, "back", dim);
    }
    return t;
}

/** Builds the pinned header: a rounded full-width panel whose content is the
 *  "Fullscreen Example - XXX" line, plus the YYY legend (below / on border). */
static ScRendered *build_header(bool finder, int count, const char *selected) {
    ScText *title = title_line(finder, count, selected);
    ScText *legend = legend_line(finder);

    ScPanelOpts po = {
        .border = { .type = SC_BORDER_ROUNDED, .color = SC_COLOR_ACCENT },
        .full_width = true,
        .padding = { .left = 1, .right = 1 },
    };
    if (!SUBTITLE_BELOW) {
        po.subtitle = (ScTitle){ .rich_text = legend, .halign = SC_ALIGN_RIGHT,
                                 .pad = 1, .pos = SC_POSITION_BOTTOM };
    }
    ScRendered *panel = sc_capture_panel_text(title, po);
    ScRendered *out = panel;
    if (SUBTITLE_BELOW) {
        ScRendered *sub = sc_capture_text(legend);
        const ScRendered *parts[2] = { panel, sub };
        ScRendered *joined = sc_vstack(parts, 2, 0);
        sc_rendered_free(sub);
        if (joined) { sc_rendered_free(panel); out = joined; }
    }
    sc_text_free(title);
    sc_text_free(legend);
    return out;
}

/** Runs the fuzzy finder full-screen under `hdr`. On a normal submit copies the
 *  chosen label into `sel`; writes the fired shortcut id (or -1) to *fired. */
static ScInputStatus run_finder(int *fired, char *sel, size_t cap,
                                const ScRendered *hdr) {
    Ctx ctx = { .next = 6 };
    ScShortcut sk[] = {
        { .chord = sc_key_ctrl('f'), .id = ACT_FORM,
          .mode = SC_SHORTCUT_RETURN, .hint_label = "edit (form)" },
        { .chord = sc_key_ctrl('r'), .id = ACT_REFRESH,
          .mode = SC_SHORTCUT_CALLBACK, .on_fire = on_refresh, .user = &ctx,
          .hint_label = "refresh" },
    };
    ScFuzzy *f = sc_fuzzy_new((ScFuzzyOpts){
        .prompt          = "Find",
        .fullscreen      = true,        /* grow to fill, then scroll */
        .valign          = VALIGN,
        .header          = hdr,
        .hide_summary    = true,
        .shortcuts       = sk,
        .n_shortcuts     = 2,
        .out_shortcut_id = fired,
        .box = { .enabled = true, .width_mode = SC_WIDTH_FULL,
                 .border = { .type = SC_BORDER_ROUNDED } },
    });
    ctx.fuzzy = f;
    for (int i = 1; i <= 5; i++) {
        char label[32];
        snprintf(label, sizeof label, "Task %d", i);
        sc_fuzzy_add(f, label);
    }
    size_t idx = 0;
    ScInputStatus st = sc_fuzzy_run(f, &idx);
    if (st == SC_INPUT_OK) {
        const char *lbl = sc_fuzzy_label(f, idx);
        snprintf(sel, cap, "%s", lbl ? lbl : "?");
    }
    sc_fuzzy_free(f);
    return st;
}

/** Runs the edit form full-screen under `hdr`, prefilled with `task`. */
static ScInputStatus run_form(const char *task, const ScRendered *hdr) {
    ScForm *form = sc_form_new((ScFormOpts){ .accent = SC_COLOR_ACCENT,
                                             .hide_summary = true,
                                             .fullscreen = true,
                                             .valign = VALIGN,
                                             .header = hdr });
    ScFieldOpts full = { .width_mode = SC_FWIDTH_PCT, .width = 100 };
    sc_form_row_begin(form);
    sc_form_add_text(form, "Title", task, full);
    sc_form_row_begin(form);
    sc_form_add_text(form, "Note", "", full);
    ScInputStatus st = sc_form_run(form);
    sc_form_free(form);
    return st;
}

int main(void) {
    if (!sc_input_available()) {
        sc_alert_warning("Run this in a real terminal (not under a pipe).");
        return 0;
    }

    sc_altscreen_begin();   /* owns the alt screen for the whole loop */

    bool finder = true;
    char selected[48] = "Task 1";
    bool running = true;
    while (running) {
        ScRendered *hdr = build_header(finder, 5, selected);
        if (finder) {
            int fired = -1;
            ScInputStatus st =
                run_finder(&fired, selected, sizeof selected, hdr);
            if (st != SC_INPUT_OK) { running = false; }   /* esc quits */
            else { finder = false; }   /* Enter or ^F: edit the selected item */
        } else {
            run_form(selected, hdr);   /* Ctrl-D saves, Esc cancels */
            finder = true;
        }
        sc_rendered_free(hdr);
    }

    sc_altscreen_end();
    return 0;
}
