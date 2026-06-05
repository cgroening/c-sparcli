#include "test_style.h"
#include "input/input_internal.h"


void style_text(void) {
    /* Default cursor block (black on white), with a value. The character
     * counter is shown by default below the field. */
    style_show("text: default cursor + counter (count only, no max)",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Name:", .initial = "Ada Lovelace" }));

    /* Empty field with a dim placeholder + default cursor. */
    style_show("text: empty with placeholder",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Email:", .placeholder = "you@example.com" }));

    /* Styled value + custom cursor (white on blue). */
    style_show("text: cyan value, cursor white-on-blue",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Host:", .initial = "localhost",
            .prompt_style = {
                SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
            },
            .value_style = {
                SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE,
            },
            .cursor_style = {
                SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_WHITE, SC_ANSI_COLOR_BLUE,
            } }));

    /* Character counter with a maximum: shows count/max. */
    style_show("text: counter with max (count/max)",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Tweet:", .initial = "hello world", .max_chars = 40 }));

    /* Counter suppressed. */
    style_show("text: counter hidden",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Note:", .initial = "no counter here",
            .hide_char_count = true }));

    /* Custom counter style. */
    style_show("text: counter styled (bold cyan), max",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Title:", .initial = "Style", .max_chars = 32,
            .count_style = {
                SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE,
            } }));

    /* Password masks (counter still shown). */
    style_show("password: default mask '*'",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Password:", .initial = "hunter2", .mask = "*" }));
    style_show("password: bullet mask",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "PIN:", .initial = "1234",
            .mask = "\xe2\x80\xa2" /* • */ }));
    style_show("password: hidden mask (length concealed)",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Secret:", .initial = "topsecret", .mask = "" }));

    /* Styled prompt label. */
    style_show("prompt_style: bold magenta label",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Username:", .initial = "neo",
            .prompt_style = {
                SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_MAGENTA, SC_ANSI_COLOR_NONE,
            } }));

    /* ── Boxed mode (panel): prompt on top, counter on bottom-right ── */

    style_show("boxed: fixed width 30, counter (count only)",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Name", .initial = "Ada Lovelace",
            .box.enabled = true, .box.width = 30 }));

    style_show("boxed: fixed width 30, counter with max",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Tweet", .initial = "hello world", .max_chars = 40,
            .box.enabled = true, .box.width = 30 }));

    style_show("boxed: counter hidden",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Note", .initial = "no counter", .hide_char_count = true,
            .box.enabled = true, .box.width = 30 }));

    /* Box background: the title prompt and the bottom-right counter must
     * inherit box.bg (border captions on the widget bg, not terminal default). */
    style_show("boxed: bg fills title + counter captions",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Name", .initial = "Ada", .max_chars = 24,
            .box.enabled = true, .box.width = 30,
            .box.bg = { .index = -1, .r = 40, .g = 44, .b = 60 } }));

    style_show("boxed: double border + cyan prompt",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Host", .initial = "localhost", .max_chars = 24,
            .box.enabled = true, .box.width = 30,
            .box.border = { .type = SC_BORDER_DOUBLE,
                            .color = SC_ANSI_COLOR_BLUE },
            .prompt_style = {
                SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE,
            } }));

    style_show("boxed: password, fixed width 30",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Password", .initial = "hunter2", .mask = "*",
            .box.enabled = true, .box.width = 30 }));

    /* Full-width box spans the whole terminal, so it is printed flush-left
     * (indenting it would push the right border past the terminal edge). */
    style_show_flush("boxed: full width (default), flush-left",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Search", .initial = "query", .box.enabled = true }));

    /* Hint layout on the boxed-footer path: stacked + hidden. */
    style_show("boxed: hint_layout stacked (footer one per line)",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Name", .initial = "Ada", .box.enabled = true, .box.width = 30,
            .hint_layout = SC_HINT_STACKED }));
    style_show("boxed: hint_layout hidden (no footer)",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Name", .initial = "Ada", .box.enabled = true, .box.width = 30,
            .hint_layout = SC_HINT_HIDDEN }));

    /* hint_pos right on a boxed field: hint stacked beside the panel. */
    style_show("boxed: hint_pos right (stacked beside the box)",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Name", .initial = "Ada", .box.enabled = true, .box.width = 30,
            .hint_pos = SC_HINT_POS_RIGHT, .hint_layout = SC_HINT_STACKED }));

    /* Autocomplete available: the default hint leads with "tab complete". */
    static const char *const regions[] = { "eu-central-1" };
    style_show("autocomplete: ghost + 'tab complete' in the default hint",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Region:", .initial = "eu-",
            .suggestions = regions, .n_suggestions = 1 }));

    /* ── Dropdown autocomplete (SC_SUGGEST_DROPDOWN) ── */

    static const char *const commands[] = {
        "commit", "checkout", "cherry-pick", "clone", "config",
    };

    /* Plain dropdown, prefix matching, no row highlighted. */
    style_show("dropdown: prefix matches, no highlight",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Git command:", .initial = "c",
            .suggestions = commands, .n_suggestions = 5,
            .suggest = { .mode = SC_SUGGEST_DROPDOWN, .max_visible = 3 } }));

    /* Highlighted row (default style: bold cyan + ‣ marker). */
    style_show("dropdown: second row highlighted (default style)",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Git command:", .initial = "c",
            .suggestions = commands, .n_suggestions = 5,
            .suggest = { .mode = SC_SUGGEST_DROPDOWN, .max_visible = 3 },
            .suggest_cursor = 2 }));

    /* Custom selected style: black-on-cyan background + custom markers. */
    style_show("dropdown: selected_style with background, custom markers",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Git command:", .initial = "ch",
            .suggestions = commands, .n_suggestions = 5,
            .suggest = {
                .mode = SC_SUGGEST_DROPDOWN,
                .selected_style = {
                    SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_BLACK, SC_ANSI_COLOR_CYAN,
                },
                .item_style = {
                    SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                },
                .cursor_marker = "> ", .marker = "  ",
            },
            .suggest_cursor = 1 }));

    /* Bordered dropdown: rounded frame around the list. */
    style_show("dropdown: rounded border frame",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Git command:", .initial = "c",
            .suggestions = commands, .n_suggestions = 5,
            .suggest = {
                .mode = SC_SUGGEST_DROPDOWN, .max_visible = 3,
                .border = { .type = SC_BORDER_ROUNDED,
                            .color = SC_ANSI_COLOR_CYAN },
            },
            .suggest_cursor = 1 }));

    /* Fuzzy matching: "cp" hits "cherry-pick" (subsequence). */
    style_show("dropdown: fuzzy matching",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Git command:", .initial = "cp",
            .suggestions = commands, .n_suggestions = 5,
            .suggest = { .mode = SC_SUGGEST_DROPDOWN,
                         .match = SC_SUGGEST_MATCH_FUZZY },
            .suggest_cursor = 1 }));

    /* Overflow: more matches than max_visible shows a dim "+N more" line. */
    style_show("dropdown: overflow line (max_visible 2 of 5 matches)",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Git command:", .initial = "c",
            .suggestions = commands, .n_suggestions = 5,
            .suggest = { .mode = SC_SUGGEST_DROPDOWN, .max_visible = 2 } }));

    /* Boxed field + bordered dropdown aligned to the box width. */
    style_show("dropdown: boxed field, dropdown frame matches box width",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Git command", .initial = "c", .box.enabled = true, .box.width = 30,
            .suggestions = commands, .n_suggestions = 5,
            .suggest = {
                .mode = SC_SUGGEST_DROPDOWN, .max_visible = 3,
                .border = { .type = SC_BORDER_ROUNDED },
            },
            .suggest_cursor = 1 }));

    /* ── Rich prompt: only part of the prompt styled (markup) ── */
    style_show("rich prompt: inline markup (italic name)",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Rename [italic]Apple[/] to", .prompt_markup = true,
            .initial = "Apple" }));
    style_show("rich prompt: boxed markup title (italic name)",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Rename [italic]Apple[/] to", .prompt_markup = true,
            .initial = "Apple", .box.enabled = true, .box.width = 36 }));
}
