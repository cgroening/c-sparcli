// textarea_editor.cpp - multi-line entry and the external-editor hook
// (C++ wrapper).
//
// Run (after `make`) in a real terminal:
//   make run-example EX=cpp/input/textarea_editor
//
// Keys: Enter inserts a newline, Ctrl-D submits, Ctrl-G opens $EDITOR.

#include <sparcli.hpp>

#include <print>

using namespace sparcli;


static void run_textarea();
static void run_editor_input();


int main() {
    if (!input_available()) {
        alert::warning("Run this example in a real terminal "
                       "(not under a pipe).");
        return 0;
    }

    run_textarea();
    run_editor_input();
    return 0;
}

// Boxed multi-line editor with soft-wrapped long lines.
static void run_textarea() {
    if (auto notes = textarea("Release notes (Ctrl-D submits)",
            { .placeholder = "What changed?",
              .box = { .enabled = true,
                       .border = { .type = SC_BORDER_ROUNDED },
                       .width = 52 } })) {
        std::println("  -> {} bytes", notes->size());
    }
}

// External editor: Ctrl-G opens $EDITOR; text_input collapses newlines.
static void run_editor_input() {
    if (auto message = text_input(
            "Commit message (Ctrl-G opens the editor)",
            { .external_editor = true, .editor_key = key_ctrl('g') })) {
        std::println("  -> \"{}\"", *message);
    }
}
