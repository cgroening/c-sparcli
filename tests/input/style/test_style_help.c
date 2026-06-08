#include "test_style.h"
#include "input/input_internal.h"


void style_help(void) {
    /* Mixed help screen: sections, bound-shortcut rows and help-only rows. */
    ScShortcutHelpRow rows[] = {
        { .section = "Navigation" },
        { .key_display = "\xe2\x86\x91/\xe2\x86\x93 or j/k",
          .desc = "move cursor" },
        { .key_display = "i", .desc = "filter" },
        { .section = "Actions" },
        { .key_display = "^E", .desc = "edit the selected task" },
        { .key_display = "^X", .desc = "delete the selected task" },
        { .section = "Other" },
        { .key_display = "?", .desc = "this help" },
        { .key_display = "q", .desc = "quit" },
    };
    size_t n = sizeof rows / sizeof rows[0];

    style_show("help: default (yellow accent)",
               sc_shortcut_help_frame(rows, n, NULL));

    style_show("help: custom title + accent",
               sc_shortcut_help_frame(rows, n, &(ScShortcutHelpOpts){
                   .title = "Tasks \xc2\xb7 shortcuts",
                   .accent = SC_ANSI_COLOR_MAGENTA }));

    /* Built straight from a bound shortcut set (grouping + key derivation). */
    ScShortcut items[] = {
        { .chord = sc_key_ctrl('n'), .id = 1, .hint_label = "next",
          .section = "Navigation" },
        { .chord = sc_key_ctrl('e'), .id = 2, .hint_label = "edit",
          .help_text = "edit the selected task", .section = "Actions" },
        { .chord = sc_key_fn(2), .id = 3, .hint_label = "help" },
    };
    style_show("help: built from bound shortcuts",
               sc_shortcut_help_frame_from(
                   items, sizeof items / sizeof items[0], NULL));
}
