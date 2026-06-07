/*
 * fullscreen_app.c
 *
 * A full-screen (alternate-screen) app shell built from the live display:
 *
 *   - sc_live (alt_screen) owns the screen; a title PANEL is pinned as the
 *     header (the live frame). The subtitle is a rich ScText line shown either
 *     on the panel's bottom border or below the panel (SUBTITLE_BELOW).
 *   - The interactive widget runs in the reserved rows below the header
 *     (prompt_rows), with hide_summary = true so it erases itself cleanly.
 *   - The whole header+content block is vertically aligned via the VALIGN knob
 *     (ScLiveOpts.valign) - visible only when the content does NOT fill the
 *     screen, i.e. with CONTENT_HEIGHT > 0.
 *   - The app switches between a FUZZY finder and a FORM: Enter (or ^F) on an
 *     item opens the form to edit it; the form returns to the finder.
 *   - The finder fills the reserved height (max_height) and scrolls (with a
 *     right-edge scrollbar) once items exceed it; ^R appends an item live.
 *
 * Build + run in a real terminal:
 *   make run-example EX=c/apps/fullscreen_app
 */

#include "sparcli.h"

#include <stdio.h>
#include <string.h>


/* ── Knobs you can change ────────────────────────────────────── */

/* Vertical alignment of the whole header+content block within the screen.
   Has a VISIBLE effect only when the block does not fill the screen, i.e. when
   CONTENT_HEIGHT > 0 below. Try SC_VALIGN_MIDDLE or SC_VALIGN_BOTTOM. */
static const ScVAlign VALIGN = SC_VALIGN_MIDDLE;

/* Height of the content (finder/form) region in rows. 0 = fill the screen
   below the header (scroll when items exceed it). A positive value caps it -
   and then VALIGN can center/bottom-align the whole block. */
static const int CONTENT_HEIGHT = 0;

/* true  = subtitle is a rich ScText line BELOW the panel.
   false = subtitle sits on the panel's bottom border (also ScText). */
static const bool SUBTITLE_BELOW = true;

/* ────────────────────────────────────────────────────────────── */

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

/* Header panel rows: 3 (border+content+border), plus 1 for a subtitle below. */
static int header_rows(void) { return SUBTITLE_BELOW ? 4 : 3; }

/** Header: a full-width title panel + a rich ScText `subtitle`, placed on the
 *  panel border or below it (SUBTITLE_BELOW). Returns the live frame. */
static ScRendered *header(const ScText *subtitle) {
    ScPanelOpts po = {
        .border = { .type = SC_BORDER_ROUNDED, .color = SC_COLOR_ACCENT },
        .full_width = true,
        .title = { .text = "mdtask shell", .halign = SC_ALIGN_LEFT, .pad = 1,
                   .style = { SC_TEXT_ATTR_BOLD, SC_COLOR_ACCENT,
                              SC_ANSI_COLOR_NONE } },
    };
    if (!SUBTITLE_BELOW) {
        /* ScText subtitle on the bottom border (panels honor rich_text). */
        po.subtitle = (ScTitle){ .rich_text = subtitle,
                                 .halign = SC_ALIGN_RIGHT,
                                 .pad = 1, .pos = SC_POSITION_BOTTOM };
    }
    ScRendered *panel =
        sc_capture_panel_str("Press the listed keys to act.", po);
    if (!SUBTITLE_BELOW || !subtitle) {
        return panel;
    }
    /* Stack the rich subtitle as its own line below the panel. */
    ScRendered *sub = sc_capture_text(subtitle);
    const ScRendered *parts[2] = { panel, sub };
    ScRendered *out = sc_vstack(parts, 2, 0);
    sc_rendered_free(sub);
    if (out) { sc_rendered_free(panel); return out; }
    return panel;
}

/** Builds the rich subtitle line for the current state. */
static ScText *subtitle_for(bool finder, const char *selected) {
    ScText *t = sc_text_new();
    ScTextStyle dim = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                        SC_ANSI_COLOR_NONE };
    ScTextStyle accent = { SC_TEXT_ATTR_BOLD, SC_COLOR_ACCENT,
                           SC_ANSI_COLOR_NONE };
    if (finder) {
        sc_text_append(t, "enter/^F edit \xc2\xb7 ^R refresh \xc2\xb7 esc quit",
                       dim);
    } else {
        sc_text_append(t, "editing ", dim);
        sc_text_append(t, selected, accent);
        sc_text_append(t, "  \xc2\xb7  ^D save \xc2\xb7 esc back", dim);
    }
    return t;
}

/** Runs the fuzzy finder. Returns the status, writes the fired shortcut id (or
 *  -1) to *fired, and on a normal submit copies the chosen label into `sel`. */
static ScInputStatus run_finder(int *fired, int reserve, char *sel,
                                size_t cap) {
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
        /* Fill the reserved height; scroll once items exceed it. */
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
    if (st == SC_INPUT_OK) {
        const char *lbl = sc_fuzzy_label(f, idx);   /* selected / cursor row */
        snprintf(sel, cap, "%s", lbl ? lbl : "?");
    }
    sc_fuzzy_free(f);
    return st;
}

/** Runs the edit form, prefilled with the selected task title. */
static ScInputStatus run_form(const char *task) {
    ScForm *form = sc_form_new((ScFormOpts){ .accent = SC_COLOR_ACCENT,
                                             .hide_summary = true });
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

    /* CONTENT_HEIGHT > 0 caps the content (then VALIGN centers the block);
       0 fills the screen below the header, with one slack row so the finder
       never scrolls the header off the top. */
    int reserve = CONTENT_HEIGHT > 0
        ? CONTENT_HEIGHT
        : sc_term_height() - header_rows() - 1;
    if (reserve < 4) { reserve = 4; }

    ScLive *live = sc_live_begin((ScLiveOpts){
        .alt_screen  = true,
        .prompt_rows = reserve,
        .valign      = VALIGN,   /* whole-block alignment (set VALIGN above) */
    });

    bool finder = true;            /* start in the fuzzy finder */
    char selected[48] = "Task 1";  /* last chosen item (edited by the form) */
    bool running = true;
    while (running) {
        ScText *sub = subtitle_for(finder, selected);
        ScRendered *h = header(sub);
        sc_live_update(live, h);
        sc_rendered_free(h);
        sc_text_free(sub);

        if (finder) {
            int fired = -1;
            ScInputStatus st = run_finder(&fired, reserve, selected,
                                          sizeof selected);
            if (st != SC_INPUT_OK) { running = false; }   /* esc quits */
            else { finder = false; }   /* Enter or ^F: edit the selected item */
        } else {
            run_form(selected);   /* Ctrl-D saves, Esc cancels */
            finder = true;        /* back to the finder */
        }
    }

    sc_live_end(live);
    return 0;
}
