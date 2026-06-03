/*
 * shortcuts_theme.c - custom keyboard shortcuts on a widget and the
 * process-wide input theme.
 *
 * Run (after `make`) in a real terminal:
 *   make run-example EX=c/input/shortcuts_theme
 *
 * Keys in the select: F2 archives (closes and reports), Ctrl-X deletes the
 * highlighted item in place, Enter picks, Esc cancels.
 */

#include <sparcli.h>

#include <stdio.h>


/** Shortcut id reported through out_shortcut_id. */
enum { ACTION_ARCHIVE = 1 };


static void apply_theme(void);
static void run_select_with_shortcuts(void);
static bool delete_highlighted(int id, void *user);


int main(void) {
    if (!sc_input_available()) {
        sc_alert_warning("Run this example in a real terminal "
                         "(not under a pipe).");
        return 0;
    }

    apply_theme();
    run_select_with_shortcuts();

    // Reset to the built-in defaults for any code that runs afterwards.
    sc_input_set_theme(NULL);
    return 0;
}

/** Process-wide defaults: every widget inherits them for zero-init opts. */
static void apply_theme(void) {
    sc_input_set_theme(&(ScInputTheme){
        .accent        = SC_ANSI_COLOR_MAGENTA,
        .prompt_style  = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_MAGENTA,
                           SC_ANSI_COLOR_NONE },
        .cursor_marker = "▶ ",
        .marker        = "  ",
        .hint_layout   = SC_HINT_INLINE,
    });
}

/** CALLBACK shortcut: remove the highlighted item, keep the prompt open. */
static bool delete_highlighted(int id, void *user) {
    (void)id;
    ScSelect *select = user;
    sc_select_remove(select, sc_select_cursor(select));
    return true;    // true = stay open
}

/** A select with one RETURN shortcut and one CALLBACK shortcut. */
static void run_select_with_shortcuts(void) {
    int fired = -1;
    ScShortcut shortcuts[] = {
        // RETURN: closes the prompt and reports the id via out_shortcut_id.
        { .chord      = sc_key_fn(2),
          .id         = ACTION_ARCHIVE,
          .mode       = SC_SHORTCUT_RETURN,
          .hint_label = "archive" },
        // CALLBACK: runs in place while the prompt stays open.
        { .chord      = sc_key_ctrl('x'),
          .mode       = SC_SHORTCUT_CALLBACK,
          .on_fire    = delete_highlighted,
          .hint_label = "delete" },
    };

    ScSelect *select = sc_select_new((ScSelectOpts){
        .prompt          = "Pick a note",
        .shortcuts       = shortcuts,
        .n_shortcuts     = 2,
        .out_shortcut_id = &fired,
    });
    // The delete callback needs the live handle.
    shortcuts[1].user = select;

    static const char *const NOTES[] = {
        "Meeting notes", "Shopping list", "Project ideas", "Travel plans",
    };
    for (size_t i = 0; i < sizeof NOTES / sizeof NOTES[0]; i++) {
        sc_select_add(select, NOTES[i]);
    }

    size_t picked = 0;
    size_t count  = 1;
    ScInputStatus status = sc_select_run(select, &picked, &count);

    if (status == SC_INPUT_OK && fired == ACTION_ARCHIVE && count > 0) {
        printf("  -> archive \"%s\"\n", sc_select_label(select, picked));
    } else if (status == SC_INPUT_OK && count > 0) {
        printf("  -> picked \"%s\"\n", sc_select_label(select, picked));
    } else if (status == SC_INPUT_OK) {
        printf("  -> nothing left to pick\n");
    }

    sc_select_free(select);
}
