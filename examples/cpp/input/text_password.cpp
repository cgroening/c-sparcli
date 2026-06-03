// text_password.cpp - text input (validation, filters, autocomplete, boxed)
// and masked password input (C++ wrapper).
//
// Run (after `make`) in a real terminal:
//   make run-example EX=cpp/input/text_password

#include <sparcli.hpp>

#include <print>

using namespace sparcli;


// Validation callback: keeps the prompt open until it returns true.
static bool validate_not_empty(const char* value, void* /*ctx*/,
                               const char** error) {
    if (value[0] == '\0') {
        *error = "Please enter a value";
        return false;
    }
    return true;
}

static void run_plain_input();
static void run_autocomplete_input();
static void run_rich_prompt();
static void run_boxed_input();
static void run_password();


int main() {
    if (!input_available()) {
        alert::warning("Run this example in a real terminal "
                       "(not under a pipe).");
        return 0;
    }

    run_plain_input();
    run_autocomplete_input();
    run_rich_prompt();
    run_boxed_input();
    run_password();
    return 0;
}

// Placeholder + validation.
static void run_plain_input() {
    if (auto name = text_input("Project name",
                               { .placeholder = "my-project",
                                 .validate = validate_not_empty })) {
        std::println("  -> \"{}\"", *name);
    }
}

// Suggestions in a fuzzy dropdown plus a no-space filter.
static void run_autocomplete_input() {
    static const char* commands[] = {
        "commit", "checkout", "cherry-pick", "clone", "config",
    };
    if (auto command = text_input("Git command",
            { .char_filter = sc_filter_no_space,
              .suggestions = commands, .n_suggestions = 5,
              .suggest = { .mode = SC_SUGGEST_DROPDOWN,
                           .match = SC_SUGGEST_MATCH_FUZZY } })) {
        std::println("  -> \"{}\"", *command);
    }
}

// Rich prompt: only part of the label styled. prompt_markup parses the prompt
// string as markup; prompt_text takes a pre-built Text for dynamic content.
static void run_rich_prompt() {
    if (auto renamed = text_input("Rename [italic]Apple[/] to",
            { .initial = "Apple", .prompt_markup = true })) {
        std::println("  -> \"{}\"", *renamed);
    }
}

// Boxed mode: panel frame, character counter, length limit.
static void run_boxed_input() {
    if (auto title = text_input("Title",
            { .placeholder = "Short and descriptive",
              .max_chars = 32, .boxed = true,
              .border = { .type = SC_BORDER_ROUNDED }, .width = 44 })) {
        std::println("  -> \"{}\"", *title);
    }
}

// Masked input; the value is never echoed.
static void run_password() {
    if (auto secret = password_input("Passphrase",
                                     { .mask = "•", .max_chars = 64 })) {
        std::println("  -> captured {} bytes (not echoed)", secret->size());
    }
}
