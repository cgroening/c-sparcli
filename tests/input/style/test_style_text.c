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
            .prompt_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .value_style  = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE },
            .cursor_style = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_WHITE, SC_ANSI_COLOR_BLUE } }));

    /* Character counter with a maximum: shows count/max. */
    style_show("text: counter with max (count/max)",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Tweet:", .initial = "hello world", .max_chars = 40 }));

    /* Counter suppressed. */
    style_show("text: counter hidden",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Note:", .initial = "no counter here", .hide_char_count = true }));

    /* Custom counter style. */
    style_show("text: counter styled (bold cyan), max",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Title:", .initial = "Style", .max_chars = 32,
            .count_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE } }));

    /* Password masks (counter still shown). */
    style_show("password: default mask '*'",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Password:", .initial = "hunter2", .mask = "*" }));
    style_show("password: bullet mask",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "PIN:", .initial = "1234", .mask = "\xe2\x80\xa2" /* • */ }));
    style_show("password: hidden mask (length concealed)",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Secret:", .initial = "topsecret", .mask = "" }));

    /* Styled prompt label. */
    style_show("prompt_style: bold magenta label",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Username:", .initial = "neo",
            .prompt_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_MAGENTA, SC_ANSI_COLOR_NONE } }));
}
