/*
 * shortcuts_help.c - per-shortcut footer/help metadata and the auto-built
 * keyboard help screen (sc_shortcut_help_show).
 *
 * Run (after `make`) in a real terminal:
 *   make run-example EX=c/input/shortcuts_help
 *
 * The picker's footer shows the visible shortcuts (F1 help, ^N new); ^X delete
 * is bound but hidden from the footer (hide_in_footer). Press F1 to open a
 * scrollable help screen that documents BOTH the bound shortcuts and the
 * built-in select keys (help-only rows), grouped into sections.
 */

#include <sparcli.h>

#include <stddef.h>


enum { ACT_HELP = 1, ACT_NEW = 2, ACT_DELETE = 3 };


int main(void) {
    if (!sc_input_available()) {
        sc_alert_warning("Run this example in a real terminal "
                         "(not under a pipe).");
        return 0;
    }

    /* Bound shortcuts: footer text (hint_label), longer help text, a section,
       and one binding kept out of the footer (hide_in_footer). */
    ScShortcut shortcuts[] = {
        { .chord = sc_key_ctrl('n'), .id = ACT_NEW, .hint_label = "new",
          .help_text = "create a new item", .section = "Actions" },
        { .chord = sc_key_ctrl('x'), .id = ACT_DELETE, .hint_label = "delete",
          .help_text = "delete the highlighted item",
          .hide_in_footer = true, .section = "Actions" },
        { .chord = sc_key_fn(1), .id = ACT_HELP, .hint_label = "help",
          .help_text = "show this keyboard help", .section = "Other" },
    };
    const size_t n_shortcuts = sizeof shortcuts / sizeof shortcuts[0];

    /* A help screen that also documents the built-in select keys, expressed as
       help-only rows (no binding). Authored in display order. */
    ScShortcutHelpRow help_rows[] = {
        { .section = "Navigation" },
        { .key_display = "\xe2\x86\x91/\xe2\x86\x93 or j/k",
          .desc = "move the cursor" },
        { .key_display = "\xe2\x86\xb5", .desc = "pick the highlighted item" },
        { .section = "Actions" },
        { .key_display = "^N", .desc = "create a new item" },
        { .key_display = "^X", .desc = "delete the highlighted item" },
        { .section = "Other" },
        { .key_display = "F1", .desc = "show this keyboard help" },
        { .key_display = "Esc", .desc = "cancel" },
    };
    const size_t n_help = sizeof help_rows / sizeof help_rows[0];

    for (;;) {
        int fired = -1;
        ScSelect *sel = sc_select_new((ScSelectOpts){
            .shortcuts = shortcuts, .n_shortcuts = n_shortcuts,
            .out_shortcut_id = &fired,
        });
        sc_select_add(sel, "Apples");
        sc_select_add(sel, "Bananas");
        sc_select_add(sel, "Cherries");
        size_t idx = 0, count = 1;
        ScInputStatus st = sc_select_run(sel, &idx, &count);
        sc_select_free(sel);

        if (st != SC_INPUT_OK) {
            break;   /* Esc / Ctrl-C */
        }
        if (fired == ACT_HELP) {
            sc_shortcut_help_show(help_rows, n_help, &(ScShortcutHelpOpts){
                .title = "Fruit picker \xc2\xb7 shortcuts" });
            continue;   /* reopen the picker */
        }
        /* A normal pick or another shortcut: report and exit. */
        sc_markup_println(fired == ACT_NEW    ? "[green]new item[/]"
                          : fired == ACT_DELETE ? "[red]delete[/]"
                          : "[cyan]picked an item[/]");
        break;
    }

    /* sc_shortcut_help_show_from builds the rows straight from a bound set
       (no built-in keys); handy when the shortcuts are the whole story:
           sc_shortcut_help_show_from(shortcuts, n_shortcuts, NULL);  */
    return 0;
}
