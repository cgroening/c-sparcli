#include "input/input_internal.h"
#include "internal.h"   /* sc_utf8_string_length */

#include <string.h>


/*
 * Auto-built keyboard-shortcut help screen.
 *
 * Rather than reimplement scrolling, sections, a scrollbar and modal key
 * handling, the help screen is a thin configuration of the fuzzy finder
 * (`src/input/fuzzy.c`) in table mode: two columns (key, description), section
 * headers, and an inline rounded box in the accent color. `sc_fuzzy_run` gives
 * the modal loop (Esc/Enter close); the selected index is ignored.
 *
 * Public API: include/input/sparcli_shortcut.h.
 */


/** Resolves the accent: caller value, or the palette yellow when unset. */
static ScColor help_accent(const ScShortcutHelpOpts *opts) {
    if (opts && opts->accent.index != 0) {
        return opts->accent;
    }
    return SC_COLOR_YELLOW;
}

/**
 * Builds the configured fuzzy finder for `rows`. The caller runs or snapshots
 * it and frees it with `sc_fuzzy_free`. Returns NULL on OOM.
 */
static ScFuzzy *build_help_fuzzy(const ScShortcutHelpRow *rows, size_t n,
                                 const ScShortcutHelpOpts *opts) {
    ScColor accent = help_accent(opts);
    const char *title = (opts && opts->title) ? opts->title
                                              : "Keyboard shortcuts";
    const char *hint = (opts && opts->footer_hint)
        ? opts->footer_hint
        : "type to filter \xc2\xb7 esc to close";   /* "·" */
    static const char *const headers[2] = { "", "" };

    ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = title,
        .accent = accent,
        .table = true,
        .headers = headers,
        .n_cols = 2,
        .hint = hint,
        /* Full terminal width so the box stays constant regardless of which
           rows are visible; the description column (col 1) absorbs the slack
           so the table fills the frame instead of resizing with the content. */
        .box = { .enabled = true,
                 .width_mode = SC_WIDTH_FULL,
                 .border = { .type = SC_BORDER_ROUNDED, .color = accent } },
        .stretch_columns = 1u << 1,
        /* Fill the terminal height (rows pinned at the top, the rest blank) for
           a typical full-screen help look. fullscreen needs an alternate screen
           - sc_shortcut_help_show opens one unless the caller already holds it. */
        .fullscreen = true,
        .valign = SC_VALIGN_TOP,
        .hide_summary = true,
        .section_counts = false,
    });
    if (!fz) {
        return NULL;
    }

    const ScTextStyle key_style = {
        SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE
    };
    const ScTextStyle section_style = {
        SC_TEXT_ATTR_BOLD, accent, SC_ANSI_COLOR_NONE
    };

    /* Pad every key to the widest key's display width so the key column stays a
       constant width no matter which rows are currently scrolled into view. */
    int key_w = 0;
    for (size_t i = 0; i < n; i++) {
        const char *k = rows[i].key_display;
        if (rows[i].section || !k) { continue; }
        int w = (int)sc_utf8_string_length(k, strlen(k));
        if (w > key_w) { key_w = w; }
    }

    for (size_t i = 0; i < n; i++) {
        const ScShortcutHelpRow *r = &rows[i];
        if (r->section) {
            sc_fuzzy_add_section_styled(fz, r->section, section_style);
            continue;
        }
        const char *k = r->key_display ? r->key_display : "";
        char keybuf[128];
        size_t len = strlen(k);
        if (len >= sizeof keybuf) { len = sizeof keybuf - 1; }
        memcpy(keybuf, k, len);
        int pad = key_w - (int)sc_utf8_string_length(k, strlen(k));
        while (pad-- > 0 && len + 1 < sizeof keybuf) { keybuf[len++] = ' '; }
        keybuf[len] = '\0';

        const char *fields[2] = { keybuf, r->desc ? r->desc : "" };
        ScTextStyle styles[2] = { key_style, (ScTextStyle){ 0 } };
        sc_fuzzy_add_row_styled(fz, fields, styles, 2);
    }
    return fz;
}

void sc_shortcut_help_show(const ScShortcutHelpRow *rows, size_t n,
                           const ScShortcutHelpOpts *opts) {
    if (!rows || n == 0) {
        return;
    }
    ScFuzzy *fz = build_help_fuzzy(rows, n, opts);
    if (!fz) {
        return;
    }
    /* fullscreen rendering needs an alternate screen. Open one ourselves unless
       the caller already holds one (in_alt_screen), so we never nest them. */
    bool own_screen = !(opts && opts->in_alt_screen);
    if (own_screen) {
        sc_altscreen_begin();
    }
    size_t idx = 0;
    sc_fuzzy_run(fz, &idx);   /* selection ignored: Esc and Enter both close */
    if (own_screen) {
        sc_altscreen_end();
    }
    sc_fuzzy_free(fz);
}


/* ── Build rows from a bound shortcut set ────────────────────────────────── */

/** Display rows derived from bound shortcuts, plus the owned key-name pool. */
typedef struct HelpBuilt {
    ScShortcutHelpRow *rows;    /**< Owned; section/desc borrow from `items`. */
    size_t             n;
    char             **keys;    /**< Owned key-name strings (one per line). */
    size_t             n_keys;
} HelpBuilt;

static void help_built_free(HelpBuilt *b) {
    for (size_t i = 0; i < b->n_keys; i++) {
        free(b->keys[i]);
    }
    free(b->keys);
    free(b->rows);
    *b = (HelpBuilt){ 0 };
}

/**
 * Derives help rows from `items` in insertion order: a section header whenever
 * the (NULL => "Other") section changes, then one key/description line per
 * shortcut whose description (`help_text` or `hint_label`) is non-empty.
 */
static HelpBuilt build_rows_from(const ScShortcut *items, size_t n) {
    HelpBuilt b = { 0 };
    if (!items || n == 0) {
        return b;
    }
    /* Worst case: a header + a line per item. */
    b.rows = calloc(n * 2, sizeof *b.rows);
    b.keys = calloc(n, sizeof *b.keys);
    if (!b.rows || !b.keys) {
        help_built_free(&b);
        return b;
    }
    const char *prev_section = NULL;
    for (size_t i = 0; i < n; i++) {
        const ScShortcut *s = &items[i];
        const char *desc = s->help_text ? s->help_text : s->hint_label;
        if (!desc || !desc[0]) {
            continue;   /* nothing to describe -> not in the help screen */
        }
        const char *section = s->section ? s->section : "Other";
        if (!prev_section || strcmp(prev_section, section) != 0) {
            b.rows[b.n++] = (ScShortcutHelpRow){ .section = section };
            prev_section = section;
        }
        char name[32];
        sc_key_chord_name(s->chord, name, sizeof name);
        char *key = sc_dup_str(name);
        b.keys[b.n_keys++] = key;
        b.rows[b.n++] = (ScShortcutHelpRow){
            .key_display = key, .desc = desc
        };
    }
    return b;
}

void sc_shortcut_help_show_from(const ScShortcut *items, size_t n,
                                const ScShortcutHelpOpts *opts) {
    HelpBuilt b = build_rows_from(items, n);
    if (b.n) {
        sc_shortcut_help_show(b.rows, b.n, opts);
    }
    help_built_free(&b);
}


/* ── Test hooks (declared in input_internal.h) ───────────────────────────── */

ScRendered *sc_shortcut_help_frame(const ScShortcutHelpRow *rows, size_t n,
                                   const ScShortcutHelpOpts *opts) {
    ScFuzzy *fz = build_help_fuzzy(rows, n, opts);
    if (!fz) {
        return NULL;
    }
    ScRendered *r = sc_fuzzy_frame(fz, "");
    sc_fuzzy_free(fz);
    return r;
}

ScRendered *sc_shortcut_help_frame_from(const ScShortcut *items, size_t n,
                                        const ScShortcutHelpOpts *opts) {
    HelpBuilt b = build_rows_from(items, n);
    ScRendered *r = b.n ? sc_shortcut_help_frame(b.rows, b.n, opts) : NULL;
    help_built_free(&b);
    return r;
}
