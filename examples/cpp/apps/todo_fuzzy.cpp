// todo_fuzzy.cpp - a mini todo app built on the fuzzy finder (C++ wrapper).
//
// Run (after `make`) in a real terminal:
//   make run-example EX=cpp/apps/todo_fuzzy
//
// Demonstrates the finder's todo-oriented features together:
//   - section headers per day (non-selectable) with per-group counts
//   - a table view ordered chronologically by the Time column
//   - per-cell status colors and a rich (multi-color) priority badge
//   - multi-select with a dedicated checkbox column
//     (Space toggles, Ctrl-A all, Ctrl-S the current day)
//   - stable task ids so actions survive deletes/reorders
//   - action shortcuts applied to the checked set in a re-run loop, mutating
//     rows live (set_disabled/set_row_style/remove)
//   - an empty-state line when the filter matches nothing
//   - a modal normal/insert mode (vim-style): starts in normal mode where bare
//     keys act (d done, x delete, c clear, j/k move, space check); press i to
//     enter insert mode and type a filter, Esc to return. The query line shows
//     the mode (color + NORMAL/INSERT badge). The same actions also have Ctrl-
//     chords that work in both modes.
//
// Normal mode: d done · x delete · c clear · j/k move · space check · i insert.
// Insert mode: type to filter · Esc back to normal. Enter finishes, Esc (in
// normal mode) cancels.

#include <sparcli.hpp>

#include <print>
#include <vector>

using namespace sparcli;


// Columns of the task table.
enum { COL_TIME, COL_TASK, COL_STATUS, N_COLS };

// Shortcut ids reported via Shortcuts::fired() (-1 = normal Enter).
enum { ACT_DONE = 1, ACT_DELETE = 2 };


// Applies the fired action to the checked tasks, mutating the finder in place.
// Done from the highest index down so deletes don't trip over shifting indices.
static void apply_action(Fuzzy& f, int action,
                         const std::vector<std::size_t>& checked) {
    if (action == ACT_DONE) {
        const TextStyle green{ SC_TEXT_ATTR_NONE, sparcli::green(), none() };
        for (std::size_t idx : checked) {
            f.set_row_style(idx, COL_STATUS, green);
            f.set_disabled(idx, true);   // also clears the check
        }
        std::println("  marked {} task(s) done", checked.size());
    } else if (action == ACT_DELETE) {
        for (std::size_t i = checked.size(); i-- > 0;) {
            f.remove(checked[i]);
        }
        std::println("  deleted {} task(s)", checked.size());
    }
}


// Seeds the finder with the day sections and their tasks (stable ids attached).
static void add_tasks(Fuzzy& f) {
    const TextStyle red{ SC_TEXT_ATTR_BOLD, sparcli::red(), none() };
    const TextStyle green{ SC_TEXT_ATTR_NONE, sparcli::green(), none() };
    std::size_t row = 0;   // running add-order index, to attach stable ids

    // ── Monday ──
    f.add_section("Monday");
    row++;
    f.add_row({ "14:00", "Review pull requests", "open" });
    f.set_id(row++, 101);
    // An overdue task painted red in the Status column.
    f.add_row_styled({ "09:00", "Pay the invoice", "overdue" },
                     { {}, {}, red });
    f.set_id(row++, 102);

    // ── Tuesday ──
    f.add_section("Tuesday");
    row++;
    // A high-priority task whose Task cell is a rich multi-color Text: a
    // white-on-red badge followed by the plain title.
    std::vector<Text> cells;
    cells.emplace_back("11:00");
    Text badge;
    badge.append(" HIGH ",
                 TextStyle{ SC_TEXT_ATTR_BOLD, white(), sparcli::red() })
         .append(" Ship the release", TextStyle{});
    cells.push_back(std::move(badge));
    cells.emplace_back("open");
    f.add_row_rich(cells);
    f.set_id(row++, 201);

    // A task already finished: greyed out and unselectable, status green.
    f.add_row_styled({ "15:30", "Write the changelog", "done" },
                     { {}, {}, green });
    f.set_id(row, 202);
    f.set_disabled(row, true);
    row++;
}


// Interactive loop: run, apply any fired action, re-run; stop on Enter/Esc.
// The Shortcuts set is borrowed by the finder, so it lives for the whole loop.
static void run_todo() {
    static const char* headers[] = { "Time", "Task", "Status" };

    // Normal-mode actions are bare keys (no modifier) — in insert mode those
    // letters type into the query instead. The Ctrl- alternates fire in both.
    Shortcuts shortcuts;
    shortcuts.on_return(key_char('d'), ACT_DONE, "done")
             .on_return(key_char('x'), ACT_DELETE, "delete")
             .on_return(key_ctrl('d'), ACT_DONE)
             .on_return(key_ctrl('x'), ACT_DELETE);

    FuzzyOpts opts{};
    opts.prompt = "Tasks";
    opts.table = true;
    opts.headers = headers;
    opts.n_cols = N_COLS;
    opts.accent = cyan();
    opts.multi = true;
    opts.checkbox_column = true;
    opts.section_counts = true;
    // Modal vim-style mode: start in normal mode (bare keys act), i to type a
    // filter, Esc to return; 'c' clears the query. Default mode tint + badge.
    opts.modal = true;
    opts.clear_key = key_char('c');
    opts.order = SC_FUZZY_ORDER_COLUMN;   // chronological per day
    opts.order_column = COL_TIME;
    // Hover (cursor row): black text on the accent background; this fg overrides
    // each cell's own color so the highlighted row stays readable.
    opts.selected_style = TextStyle{ SC_TEXT_ATTR_NONE, black(), none() };
    // Day headers: bold white on a filled slate bar (the bg spans the row).
    opts.section_style = TextStyle{ SC_TEXT_ATTR_BOLD, white(), rgb(58, 64, 92) };
    opts.toggle_all_key = key_ctrl('a');
    opts.toggle_section_key = key_ctrl('s');
    opts.empty_text = "No matching tasks";
    opts.box = BoxStyle{ .enabled = true,
                         .border = { .type = SC_BORDER_ROUNDED,
                                     .color = cyan() },
                         .padding = { .right = 1, .left = 1 } };
    shortcuts.apply(opts);

    Fuzzy f(opts);
    add_tasks(f);

    for (;;) {
        auto checked = f.run_multi();
        if (!checked) {                 // Esc / Ctrl-C
            std::println("  cancelled");
            break;
        }
        int action = shortcuts.fired();
        if (action == ACT_DONE || action == ACT_DELETE) {
            apply_action(f, action, *checked);
            continue;                   // re-run with the mutated task list
        }
        // Normal Enter: report the finished selection by stable id.
        std::print("  finishing with {} checked task(s):", checked->size());
        for (std::size_t idx : *checked) {
            std::print(" #{}", f.id_at(idx));
        }
        std::println("");
        break;
    }
}


int main() {
    if (!input_available()) {
        alert::warning("Run this example in a real terminal "
                       "(not under a pipe).");
        return 0;
    }
    run_todo();
    return 0;
}
