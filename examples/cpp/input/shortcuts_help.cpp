// shortcuts_help.cpp - per-shortcut footer/help metadata and the auto-built
// keyboard help screen, via the C++ Shortcuts builder.
//
// Run (after `make`) in a real terminal:
//   make run-example EX=cpp/input/shortcuts_help
//
// The picker's footer shows the visible shortcuts (F1 help, ^N new); ^X delete
// is bound but hidden from the footer ({.in_footer=false}). Press F1 to open
// the scrollable help screen built from the same set, with section() headers
// and help_row() lines documenting the built-in select keys.

#include <sparcli.hpp>

namespace sc = sparcli;

enum { ACT_HELP = 1, ACT_NEW = 2, ACT_DELETE = 3 };

int main() {
    if (!sc::input_available()) {
        sc::alert::show(SC_ALERT_WARNING,
                        "Run this example in a real terminal (not a pipe).");
        return 0;
    }

    // One builder drives both the footer and the help screen. section() groups
    // the entries added after it; help_row() adds help-only (unbound) rows.
    sc::Shortcuts sh;
    sh.section("Actions")
      .on_return(sc::key_ctrl('n'), ACT_NEW,
                 {.footer = "new", .help = "create a new item"})
      .on_return(sc::key_ctrl('x'), ACT_DELETE,
                 {.footer = "delete", .help = "delete the highlighted item",
                  .in_footer = false})
      .section("Other")
      .on_return(sc::key_fn(1), ACT_HELP,
                 {.footer = "help", .help = "show this keyboard help"})
      .section("Navigation")
      .help_row("↑/↓ or j/k", "move the cursor")
      .help_row("↵", "pick the highlighted item")
      .help_row("Esc", "cancel");

    for (;;) {
        sc::SelectOpts opts{};
        sh.apply(opts);
        sc::Select sel(opts);
        sel.add("Apples").add("Bananas").add("Cherries");
        auto picked = sel.run_one();
        if (!picked) {
            break;   // Esc / Ctrl-C
        }
        if (sh.fired() == ACT_HELP) {
            sc::show_shortcuts(sh, {.title = "Fruit picker · shortcuts"});
            continue;   // reopen the picker
        }
        sc::println(sh.fired() == ACT_NEW      ? "new item"
                    : sh.fired() == ACT_DELETE ? "delete"
                                               : "picked an item");
        break;
    }
    return 0;
}
