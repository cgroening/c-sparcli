/*
 * Demo: custom shortcuts on an interactive select, including live editing.
 *
 *   make run-example EX=shortcut_demo     # run it in a real terminal
 *
 * Keys:
 *   - ↑/↓ (or j/k)  move
 *   - Enter         pick the highlighted item
 *   - F2            rename it: the list closes, a text field (pre-filled with
 *                   the current name) opens, and the list re-opens updated.
 *   - Ctrl-X        delete the highlighted item in place (list stays open)
 *   - Esc / Ctrl-C  cancel
 *
 * Why F2 closes the list to edit: a shortcut callback runs *inside* the prompt
 * session, which is single-instance, so it cannot open a second prompt. Instead
 * F2 is a RETURN-mode shortcut — it ends the select, the caller runs the text
 * prompt, updates the label, and re-opens the select at the same position.
 */

#include <sparcli.h>

#include <stdio.h>
#include <stdlib.h>


/* CALLBACK shortcut: delete the highlighted item, then stay open. The select
 * handle is passed through the shortcut's `user` pointer. */
static bool delete_cb(int id, void *user) {
    (void)id;
    ScSelect *sel = user;
    sc_select_remove(sel, sc_select_cursor(sel));
    return true;   /* keep the prompt open */
}

enum { ACTION_RENAME = 1 };

int main(void) {
    sc_println("sparcli – custom shortcut demo",
               (ScTextStyle){ SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE });

    int fired = -1;
    ScShortcut shortcuts[] = {
        { .chord      = sc_key_fn(2),
          .id         = ACTION_RENAME,
          .mode       = SC_SHORTCUT_RETURN,
          .hint_label = "rename" },
        { .chord      = sc_key_ctrl('x'),
          .mode       = SC_SHORTCUT_CALLBACK,
          .on_fire    = delete_cb,
          .hint_label = "delete" },
    };

    ScSelect *sel = sc_select_new((ScSelectOpts){
        .prompt          = "Pick a fruit",
        .shortcuts       = shortcuts,
        .n_shortcuts     = 2,
        .out_shortcut_id = &fired,
        .hide_summary    = true,   /* keep the demo's own messages unambiguous */
    });
    shortcuts[1].user = sel;       /* the delete callback needs the live handle */

    const char *fruits[] = { "Apple", "Banana", "Cherry", "Date", "Elderberry" };
    for (size_t i = 0; i < sizeof fruits / sizeof fruits[0]; i++) {
        sc_select_add(sel, fruits[i]);
    }

    /* Re-open the list after each F2 rename until Enter picks or the user cancels. */
    for (;;) {
        size_t idx[1] = { 0 };
        size_t n = 1;
        ScInputStatus status = sc_select_run(sel, idx, &n);

        if (status == SC_INPUT_ERROR) {
            sc_println("No interactive terminal available.", (ScTextStyle){ 0 });
            break;
        }
        if (status == SC_INPUT_CANCELLED) {
            sc_println("Cancelled (Esc / Ctrl-C).", (ScTextStyle){ 0 });
            break;
        }
        if (fired == ACTION_RENAME && n > 0) {
            size_t cur = idx[0];
            const char *current = sc_select_label(sel, cur);
            // The text-input prompt is a single styled string (one prompt_style
            // for the whole thing), so to italicize *only* the old name we wrap
            // it in inline italic on/off escapes (\e[3m … \e[23m).
            char prompt[128];
            snprintf(prompt, sizeof prompt, "Rename \033[3m%s\033[23m to",
                     current ? current : "");
            char *renamed = NULL;
            ScInputStatus ed = sc_text_input(prompt, &renamed,
                (ScTextInputOpts){ .initial = current });
            if (ed == SC_INPUT_OK && renamed && renamed[0]) {
                sc_select_set_label(sel, cur, renamed);
            }
            free(renamed);
            sc_select_set_cursor(sel, cur);   /* restore the highlight */
            continue;                          /* re-open the updated list */
        }
        if (n > 0) {
            printf("Picked: %s (index %zu)\n", sc_select_label(sel, idx[0]), idx[0]);
        } else {
            sc_println("Nothing selected (you deleted them all).", (ScTextStyle){ 0 });
        }
        break;
    }

    sc_select_free(sel);
    return 0;
}
