/*
 * todo_fuzzy.c - a mini todo app built on the fuzzy finder.
 *
 * Run (after `make`) in a real terminal:
 *   make run-example EX=c/apps/todo_fuzzy
 *
 * Demonstrates the finder's todo-oriented features together:
 *   - section headers per day (non-selectable) with per-group counts
 *   - a table view ordered chronologically by the Time column
 *   - per-cell status colors and a rich (multi-color) priority badge
 *   - multi-select with a dedicated checkbox column
 *     (Space toggles, Ctrl-A all, Ctrl-S the current day)
 *   - stable task ids so actions survive deletes/reorders
 *   - action shortcuts applied to the checked set in a re-run loop, mutating
 *     rows live (set_disabled/set_row_style/remove)
 *   - an empty-state line when the filter matches nothing
 *   - a modal normal/insert mode (vim-style): the finder starts in normal
 *     mode where bare keys act (d done, x delete, c clear, j/k move, space
 *     check); press i to enter insert mode and type a filter, Esc to return.
 *     The query line shows the mode (color + NORMAL/INSERT badge). The same
 *     actions also have Ctrl- chords that work in both modes.
 *
 * Normal mode: d done · x delete · c clear · j/k move · space check · i insert.
 * Insert mode: type to filter · Esc back to normal. Enter finishes, Esc (in
 * normal mode) cancels.
 */

#include <sparcli.h>

#include <stdio.h>
#include <string.h>


/* Columns of the task table. */
enum { COL_TIME, COL_TASK, COL_STATUS, N_COLS };

/* Shortcut ids reported via out_shortcut_id (-1 = normal Enter). */
enum { ACT_DONE = 1, ACT_DELETE = 2 };

static const char *const HEADERS[N_COLS] = { "Time", "Task", "Status" };


/* Seeds a finder with the day sections and their tasks. `out_action` receives
 * the fired shortcut id on each run (-1 = normal Enter). Caller frees it. */
static ScFuzzy *build_finder(int *out_action) {
    static const ScShortcut SHORTCUTS[] = {
        /* Normal-mode actions: bare keys (no modifier). In insert mode these
         * letters type into the query instead of firing. */
        { .chord = { .key = SC_KEY_CHAR, .codepoint = 'd' },
          .id = ACT_DONE, .mode = SC_SHORTCUT_RETURN,
          .hint_label = "done" },
        { .chord = { .key = SC_KEY_CHAR, .codepoint = 'x' },
          .id = ACT_DELETE, .mode = SC_SHORTCUT_RETURN,
          .hint_label = "delete" },
        /* Ctrl- alternates that fire in both normal and insert mode. */
        { .chord = { .key = SC_KEY_CHAR, .codepoint = 'd',
                     .mods = SC_MOD_CTRL },
          .id = ACT_DONE, .mode = SC_SHORTCUT_RETURN },
        { .chord = { .key = SC_KEY_CHAR, .codepoint = 'x',
                     .mods = SC_MOD_CTRL },
          .id = ACT_DELETE, .mode = SC_SHORTCUT_RETURN },
    };

    ScFuzzy *f = sc_fuzzy_new((ScFuzzyOpts){
        .prompt          = "Tasks",
        .table           = true,
        .headers         = HEADERS,
        .n_cols          = N_COLS,
        .accent          = SC_ANSI_COLOR_CYAN,
        .multi           = true,
        .checkbox_column = true,
        .section_counts  = true,
        /* Modal vim-style mode: start in normal mode (bare keys act), press i
         * to type a filter, Esc to return. 'c' clears the query in normal
         * mode. The query line is tinted + badged per mode (default colors). */
        .modal           = true,
        .clear_key       = { .key = SC_KEY_CHAR, .codepoint = 'c' },
        .order           = SC_FUZZY_ORDER_COLUMN,  /* chronological per day */
        .order_column    = COL_TIME,
        /* Hover (cursor row): black text on the accent background. On the
         * cursor row this fg overrides each cell's own color (merge_style),
         * keeping the highlighted row readable. */
        .selected_style  = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_BLACK,
                             SC_ANSI_COLOR_NONE },
        /* Day headers get their own look: bold white on a filled slate bar
         * (a section_style.bg spans the whole row). Any ScTextStyle works -
         * attributes, foreground and background are all honored. */
        .section_style   = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_WHITE,
                             sc_color_from_rgb(58, 64, 92) },
        .toggle_all_key     = sc_key_ctrl('a'),
        .toggle_section_key = sc_key_ctrl('s'),
        .empty_text      = "No matching tasks",
        .shortcuts       = SHORTCUTS,
        .n_shortcuts     = sizeof SHORTCUTS / sizeof SHORTCUTS[0],
        .out_shortcut_id = out_action,
        .box = {
            .enabled = true,
            .border  = { .type = SC_BORDER_ROUNDED,
                         .color = SC_ANSI_COLOR_CYAN },
            .padding = { .left = 1, .right = 1 },
        },
    });

    const ScTextStyle red   = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_RED,
                                SC_ANSI_COLOR_NONE };
    const ScTextStyle green = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_GREEN,
                                SC_ANSI_COLOR_NONE };
    size_t row = 0;   /* running add-order index, to attach stable ids */

    /* Per-section styling: each header gets its own full-width colored bar
       (sc_fuzzy_add_section_styled). Tab / Shift-Tab jump between sections. */
    const ScTextStyle mon_hdr = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_WHITE,
                                  SC_COLOR_ACCENT_DARK };
    const ScTextStyle tue_hdr = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_WHITE,
                                  SC_COLOR_BG_LIGHTEN_2 };

    /* ── Monday ── */
    sc_fuzzy_add_section_styled(f, "Monday", mon_hdr);
    row++;
    sc_fuzzy_add_row(f,
        (const char *[]){ "14:00", "Review pull requests", "open" }, N_COLS);
    sc_fuzzy_set_id(f, row++, 101);
    /* An overdue task painted red in the Status column. */
    sc_fuzzy_add_row_styled(f,
        (const char *[]){ "09:00", "Pay the invoice", "overdue" },
        (ScTextStyle[]){ { 0 }, { 0 }, red }, N_COLS);
    sc_fuzzy_set_id(f, row++, 102);

    /* ── Tuesday ── */
    sc_fuzzy_add_section_styled(f, "Tuesday", tue_hdr);
    row++;
    /* A high-priority task whose Task cell is a rich multi-color ScText: a
     * white-on-red badge followed by the plain title. */
    ScText *badge = sc_text_new();
    sc_text_append(badge, " HIGH ", (ScTextStyle){ SC_TEXT_ATTR_BOLD,
                   SC_ANSI_COLOR_WHITE, SC_ANSI_COLOR_RED });
    sc_text_append(badge, " Ship the release", (ScTextStyle){ 0 });
    ScText *t_time = sc_text_from_str("11:00");
    ScText *t_stat = sc_text_from_str("open");
    sc_fuzzy_add_row_rich(f, (ScText *[]){ t_time, badge, t_stat }, N_COLS);
    sc_fuzzy_set_id(f, row++, 201);
    sc_text_free(t_time);
    sc_text_free(badge);
    sc_text_free(t_stat);

    /* A task already finished: greyed out and unselectable, status green. */
    sc_fuzzy_add_row_styled(f,
        (const char *[]){ "15:30", "Write the changelog", "done" },
        (ScTextStyle[]){ { 0 }, { 0 }, green }, N_COLS);
    sc_fuzzy_set_id(f, row, 202);
    sc_fuzzy_set_disabled(f, row, true);
    row++;

    return f;
}


/* Applies the fired action to the checked tasks, mutating the finder in place.
 * Ids are resolved up front so deletes don't trip over shifting indices. */
static void apply_action(ScFuzzy *f, int action,
                         const size_t *checked, size_t n_checked) {
    if (action == ACT_DONE) {
        const ScTextStyle green = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_GREEN,
                                    SC_ANSI_COLOR_NONE };
        for (size_t i = 0; i < n_checked; i++) {
            size_t idx = checked[i];
            sc_fuzzy_set_row_style(f, idx, COL_STATUS, green);
            sc_fuzzy_set_disabled(f, idx, true);   /* also clears the check */
        }
        printf("  marked %zu task(s) done\n", n_checked);
    } else if (action == ACT_DELETE) {
        /* Remove from the highest index down so the lower ones stay valid. */
        for (size_t i = n_checked; i-- > 0; ) {
            sc_fuzzy_remove(f, checked[i]);
        }
        printf("  deleted %zu task(s)\n", n_checked);
    }
}


/* Interactive loop: run, apply any fired action, re-run; stop on Enter/Esc.
 * One finder is reused across runs so the done/delete mutations persist. */
static void run_todo(void) {
    int action = -1;
    ScFuzzy *f = build_finder(&action);

    for (;;) {
        action = -1;
        size_t checked[16];
        size_t n = sizeof checked / sizeof checked[0];
        ScInputStatus st = sc_fuzzy_run_multi(f, checked, &n);
        if (st != SC_INPUT_OK) {       /* Esc / Ctrl-C */
            printf("  cancelled\n");
            break;
        }
        if (action == ACT_DONE || action == ACT_DELETE) {
            apply_action(f, action, checked, n);
            continue;                  /* re-run with the mutated task list */
        }
        /* Normal Enter: report the finished selection by stable id. */
        printf("  finishing with %zu checked task(s):", n);
        for (size_t i = 0; i < n; i++) {
            printf(" #%llu",
                   (unsigned long long)sc_fuzzy_id_at(f, checked[i]));
        }
        printf("\n");
        break;
    }
    sc_fuzzy_free(f);
}


int main(void) {
    if (!sc_input_available()) {
        sc_alert_warning("Run this example in a real terminal "
                         "(not under a pipe).");
        return 0;
    }
    run_todo();
    return 0;
}
