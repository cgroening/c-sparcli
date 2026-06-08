#include "test_input.h"
#include "sparcli.h"

#include <stdlib.h>
#include <string.h>


/* Internal symbols (exported, declared in input_internal.h). */
int sc_shortcut_hint_rows(const ScShortcut *items, size_t n,
                          int indent, int term_w);
ScRendered *sc_shortcut_help_frame(
    const ScShortcutHelpRow *rows, size_t n, const ScShortcutHelpOpts *opts);
ScRendered *sc_shortcut_help_frame_from(
    const ScShortcut *items, size_t n, const ScShortcutHelpOpts *opts);


/* True when any line of `frame` (ANSI-stripped) contains `needle`. */
static bool help_has(const ScRendered *frame, const char *needle) {
    if (!frame) { return false; }
    for (size_t i = 0; i < frame->line_count; i++) {
        char *plain = sc_strip_ansi(frame->lines[i]);
        bool hit = plain && strstr(plain, needle) != NULL;
        free(plain);
        if (hit) { return true; }
    }
    return false;
}


void test_shortcut_help(void) {
    /* ── Footer visibility (hint_label + hide_in_footer) ── */
    {
        ScShortcut shown = {
            .chord = sc_key_ctrl('e'), .id = 1, .hint_label = "edit",
        };
        CHECK(sc_shortcut_hint_rows(&shown, 1, 0, 80) >= 1,
              "a labeled shortcut occupies the footer");

        ScShortcut hidden = {
            .chord = sc_key_ctrl('e'), .id = 1, .hint_label = "edit",
            .hide_in_footer = true,
        };
        CHECK(sc_shortcut_hint_rows(&hidden, 1, 0, 80) == 0,
              "hide_in_footer drops a labeled shortcut from the footer");

        ScShortcut unlabeled = { .chord = sc_key_ctrl('e'), .id = 1 };
        CHECK(sc_shortcut_hint_rows(&unlabeled, 1, 0, 80) == 0,
              "a shortcut with no label is not in the footer");
    }

    /* ── help_show_from: grouping, fallback, hidden-still-listed ── */
    {
        ScShortcut items[] = {
            { .chord = sc_key_ctrl('n'), .id = 1, .hint_label = "next",
              .section = "Navigation" },
            { .chord = sc_key_ctrl('e'), .id = 2, .hint_label = "edit",
              .help_text = "edit the selected task", .section = "Actions" },
            /* hidden from footer but must still appear in the help screen */
            { .chord = sc_key_ctrl('x'), .id = 3, .hint_label = "delete",
              .hide_in_footer = true, .section = "Actions" },
            /* no section -> grouped under "Other" */
            { .chord = sc_key_fn(2), .id = 4, .hint_label = "help" },
            /* no description -> skipped entirely */
            { .chord = sc_key_ctrl('w'), .id = 5 },
        };
        ScRendered *fr = sc_shortcut_help_frame_from(
            items, sizeof items / sizeof items[0], NULL);
        CHECK(fr != NULL && fr->line_count > 0,
              "help_frame_from renders a non-empty frame");
        CHECK(help_has(fr, "Navigation") && help_has(fr, "Actions")
              && help_has(fr, "Other"),
              "sections are grouped (Navigation / Actions / Other)");
        CHECK(help_has(fr, "edit the selected task"),
              "help_text is shown when set");
        CHECK(help_has(fr, "next"),
              "hint_label is the help description fallback");
        CHECK(help_has(fr, "delete"),
              "a hide_in_footer shortcut still appears in the help screen");
        CHECK(help_has(fr, "^E") && help_has(fr, "F2"),
              "derived key names appear in the key column");
        CHECK(!help_has(fr, "next-and-edit"),
              "a shortcut with no description is skipped");
        sc_rendered_free(fr);
    }

    /* ── help-only rows + explicit row model ── */
    {
        ScShortcutHelpRow rows[] = {
            { .section = "Navigation" },
            { .key_display = "\xe2\x86\x91/\xe2\x86\x93 or j/k",
              .desc = "move cursor" },
            { .key_display = "i", .desc = "filter" },
            { .section = "Other" },
            { .key_display = "q", .desc = "quit" },
        };
        ScShortcutHelpOpts opts = { .title = "Keys" };
        ScRendered *fr = sc_shortcut_help_frame(
            rows, sizeof rows / sizeof rows[0], &opts);
        CHECK(fr != NULL && help_has(fr, "move cursor")
              && help_has(fr, "filter") && help_has(fr, "quit"),
              "help-only rows render their key/description");
        CHECK(help_has(fr, "Keys"),
              "the title is shown (as the search-field label)");
        sc_rendered_free(fr);
    }
}
