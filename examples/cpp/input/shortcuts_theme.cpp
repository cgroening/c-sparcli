// shortcuts_theme.cpp - custom keyboard shortcuts and the input theme
// (C++ wrapper).
//
// Run (after `make`) in a real terminal:
//   make run-example EX=cpp/input/shortcuts_theme
//
// Keys: F2 archives (closes, reports), Ctrl-X deletes in place, Enter picks.

#include <sparcli.hpp>

#include <print>

using namespace sparcli;


// Shortcut id reported by Shortcuts::fired().
constexpr int kActionArchive = 1;


static void apply_theme();
static void run_select_with_shortcuts();


int main() {
    if (!input_available()) {
        alert::warning("Run this example in a real terminal "
                       "(not under a pipe).");
        return 0;
    }

    apply_theme();
    run_select_with_shortcuts();
    reset_theme();
    return 0;
}

// Process-wide defaults inherited by every widget for zero-init opts.
static void apply_theme() {
    set_theme({ .accent = magenta(),
                .prompt_style = style(SC_TEXT_ATTR_BOLD, magenta()),
                .cursor_marker = "▶ ", .marker = "  ",
                .hint_layout = SC_HINT_INLINE });
}

// A select with a RETURN shortcut (F2) and a CALLBACK shortcut (Ctrl-X).
static void run_select_with_shortcuts() {
    Select select({ .prompt = "Pick a note" });
    select.add("Meeting notes").add("Shopping list")
          .add("Project ideas").add("Travel plans");

    Shortcuts shortcuts;
    shortcuts.on_return(key_fn(2), kActionArchive, "archive")
             .on_callback(key_ctrl('x'), [&] {
                 // Runs in place; returning true keeps the prompt open.
                 select.remove(select.cursor());
                 return true;
             }, "delete");

    SelectOpts opts{ .prompt = "Pick a note" };
    shortcuts.apply(opts);
    Select wired(opts);
    wired.add("Meeting notes").add("Shopping list")
         .add("Project ideas").add("Travel plans");

    auto picked = wired.run_one();
    if (picked && shortcuts.fired() == kActionArchive) {
        std::println("  -> archive \"{}\"",
                     std::string(wired.label(*picked)));
    } else if (picked) {
        std::println("  -> picked \"{}\"",
                     std::string(wired.label(*picked)));
    }
}
