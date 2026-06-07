/*
 * fullscreen_app.c
 *
 * A full-screen (alternate-screen) app shell built from the live display:
 *
 *   - sc_live (alt_screen) owns the screen; a title/subtitle PANEL is pinned as
 *     the header (the live frame).
 *   - The interactive widget runs in the reserved rows below the header
 *     (prompt_rows), with hide_summary = true so it erases itself cleanly.
 *   - ScLiveOpts.valign centers the whole block vertically (SC_VALIGN_MIDDLE);
 *     set .valign_fixed_header to instead pin the header at the top and align
 *     only the widget beneath it.
 *   - The app switches between a FUZZY finder and a FORM and back (caller
 *     loop + RETURN-mode shortcut).
 *   - The fuzzy finder is capped to the reserved height (max_height) so it
 *     scrolls instead of overflowing, and a "refresh" key (CALLBACK
 *     shortcut) appends an item live via sc_fuzzy_refresh().
 *
 * Build + run in a real terminal:
 *   make run-example EX=c/apps/fullscreen_app
 */

#include "sparcli.h"

#include <stdio.h>
#include <string.h>


/* The title/subtitle panel is 3 rows (top border + content + bottom border). */
enum { HEADER_ROWS = 3 };

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

/** Header panel (title + subtitle) drawn as the pinned live frame. */
static ScRendered *header(const char *subtitle) {
    return sc_capture_panel_str("Press the listed keys to act.", (ScPanelOpts){
        .border = { .type = SC_BORDER_ROUNDED, .color = SC_COLOR_ACCENT },
        .full_width = true,
        .title    = { .text = "mdtask shell", .halign = SC_ALIGN_LEFT, .pad = 1,
                      .style = { SC_TEXT_ATTR_BOLD, SC_COLOR_ACCENT,
                                 SC_ANSI_COLOR_NONE } },
        .subtitle = { .text = subtitle, .halign = SC_ALIGN_RIGHT, .pad = 1,
                      .pos = SC_POSITION_BOTTOM,
                      .style = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                                 SC_ANSI_COLOR_NONE } },
    });
}

/** Runs the fuzzy finder in the reserved region. Returns the prompt status and
 *  writes the fired shortcut id (or -1) to *fired. */
static ScInputStatus run_finder(int *fired, int reserve) {
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
        /* Fill the screen below the header; scroll only once items exceed it.
           Pass a smaller fixed value here to cap the finder height instead. */
        .max_height      = reserve,
        .hide_summary    = true,               /* required: no leftovers */
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
    sc_fuzzy_free(f);
    return st;
}

/** Runs a small form in the reserved region. */
static ScInputStatus run_form(void) {
    ScForm *form = sc_form_new((ScFormOpts){ .accent = SC_COLOR_ACCENT,
                                             .hide_summary = true });
    ScFieldOpts full = { .width_mode = SC_FWIDTH_PCT, .width = 100 };
    sc_form_row_begin(form);
    sc_form_add_text(form, "Title", "", full);
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

    /* Reserve the whole area below the header for the widget, so the finder
       fills the screen and only scrolls when its items exceed it. Header and
       content stay together (whole-block is the default valign scope). */
    /* Leave one row of slack below so the finder filling its full height never
       scrolls the terminal and pushes the header off the top. */
    int reserve = sc_term_height() - HEADER_ROWS - 1;
    if (reserve < 4) { reserve = 4; }

    ScLive *live = sc_live_begin((ScLiveOpts){
        .alt_screen  = true,
        .prompt_rows = reserve,
        /* whole-block valign is the default; with a full-height reserve the
           block already fills the screen, header joined to the content. */
    });

    bool finder = true;   /* start in the fuzzy finder */
    bool running = true;
    while (running) {
        ScRendered *h = header(finder ? "finder  ·  ^F edit  ·  esc quit"
                                      : "form  ·  ^D save  ·  esc back");
        sc_live_update(live, h);
        sc_rendered_free(h);

        if (finder) {
            int fired = -1;
            ScInputStatus st = run_finder(&fired, reserve);
            if (st != SC_INPUT_OK) { running = false; }      /* esc quits */
            else if (fired == ACT_FORM) { finder = false; }  /* ^F -> form */
        } else {
            run_form();        /* Ctrl-D saves, Esc cancels; either returns */
            finder = true;     /* always back to the finder */
        }
    }

    sc_live_end(live);
    return 0;
}
